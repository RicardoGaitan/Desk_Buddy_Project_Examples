/*


VERY VERY IMPORTANT FROM ADMIN:
MAKE SURE TO CHANGE THE WIFI_SSID and WIFI_PASS to your own WiFi network name and password before 
uploading the code to the board. Otherwise, the weather station will not work.



  Board   : Seeed XIAO ESP32-C3
  Display : 1.3-inch 128x64 OLED (SH1106 I2C controller)

  LIBRARIES (nothing extra to install):
    - Adafruit SH110X
    - Adafruit GFX Library
    - Adafruit BusIO
    - WiFi, HTTPClient, WiFiClientSecure (built into the ESP32 core)

  CONTROLS:
    TOUCH : D1 (TTP223B capacitive touch sensor)
      - On the weather screens: tap to change page
        (Weather -> Details -> Network)
      - On an error screen: tap to retry immediately

  HOW IT WORKS:
    1. Scans for your WiFi network
    2. Connects to it
    3. Finds your approximate location + timezone from your IP address
       (ip-api.com, no account needed)
    4. Downloads the weather for that location
       (open-meteo.com, no account needed)
    5. Shows area, time, temperature, conditions with an animated icon
    The weather refreshes automatically every 10 minutes.

  WIRING:
    OLED VCC -> 3V3
    OLED GND -> GND
    OLED SDA -> D8 (GPIO8)
    OLED SCL -> D9 (GPIO9)

    TTP223B VCC -> 3V3
    TTP223B GND -> GND
    TTP223B I/O -> D1 (GPIO3)
    Buzzer (+)  -> D0 (GPIO2)
    Buzzer (-)  -> GND
*/

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// =====================================================
//   CUSTOM TYPES (must be above all functions so the
//   Arduino auto-prototype feature can see them)
// =====================================================

typedef bool (*NetFn)();

enum StatusIcon {
  ICON_SCAN,
  ICON_CONNECT,
  ICON_WIFI_OK,
  ICON_DONE,
  ICON_FAIL,
  ICON_LOCATE,
  ICON_WEATHER
};

enum WeatherKind {
  WX_CLEAR,
  WX_PARTLY,
  WX_CLOUD,
  WX_RAIN,
  WX_STORM,
  WX_SNOW,
  WX_FOG
};

// =====================================================
//                 USER SETTINGS (EDIT ME)
// =====================================================

const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

const bool USE_FAHRENHEIT = false;   // false = Celsius + km/h, true = Fahrenheit + mph
const bool USE_24H_CLOCK  = false;   // false = 12-hour with AM/PM

const unsigned long WEATHER_REFRESH_MS = 10UL * 60UL * 1000UL;  // 10 minutes
const unsigned long RETRY_MS = 10000;                           // wait before retry

// IP-based location can be off by a few km (it finds your internet
// provider's hub). For an exact location, set this to true and fill in
// your own details.
const bool        USE_MANUAL_LOCATION = false;
const char*       MANUAL_CITY         = "My Town";
const char*       MANUAL_REGION       = "My Region";
const char*       MANUAL_COUNTRY      = "My Country";
const float       MANUAL_LAT          = 0.0;
const float       MANUAL_LON          = 0.0;
const long        MANUAL_UTC_OFFSET_S = 0;   // seconds, e.g. UTC-4 = -14400

// =====================================================
//                    OLED DISPLAY
// =====================================================

#define SCREEN_W 128
#define SCREEN_H 64

Adafruit_SH1106G display(SCREEN_W, SCREEN_H, &Wire, -1);

#define WHITE SH110X_WHITE
#define BLACK SH110X_BLACK

// ---------- PINS ----------

const int PIN_JUMP = 7;   // D1 = GPIO7 (touch sensor)
const int PIN_BUZZ = 2;   // D0 = GPIO2

// TTP223B output goes HIGH when touched
const int PRESSED = HIGH;

bool isPressed(int pin) {
  return digitalRead(pin) == PRESSED;
}

// =====================================================
//                    GLOBAL DATA
// =====================================================

const unsigned long FRAME_MS = 80;

char gCity[32]    = "";
char gRegion[32]  = "";
char gCountry[32] = "";
float gLat = 0, gLon = 0;
long  gUtcOffset = 0;

float wTemp = 0, wFeels = 0, wHi = 0, wLo = 0, wWind = 0;
int   wHum = 0, wCode = 0;
bool  wIsDay = true;

bool haveLocation = false;
bool haveWeather  = false;

unsigned long lastWeatherOk = 0;
unsigned long nextFetchAt   = 0;
bool bgRunning = false;

char gErr[24] = "";

const char  TEMP_UNIT = USE_FAHRENHEIT ? 'F' : 'C';
const char* WIND_UNIT = USE_FAHRENHEIT ? "mph" : "km/h";

// =====================================================
//                 SMALL HELPERS
// =====================================================

void setErr(const char* s) {
  snprintf(gErr, sizeof(gErr), "%s", s);
}

void beepOk() {
  tone(PIN_BUZZ, 1000, 80);
  delay(110);
  tone(PIN_BUZZ, 1500, 120);
}

void beepFail() {
  tone(PIN_BUZZ, 300, 300);
}

void beepTap() {
  tone(PIN_BUZZ, 800, 30);
}

void centerText(const char* s, int y) {
  char b[22];
  snprintf(b, sizeof(b), "%s", s);       // clip to 21 characters
  int w = strlen(b) * 6;
  int x = (SCREEN_W - w) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(b);
}

// ---------- tiny JSON helpers (no extra library needed) ----------

int findValue(const String& s, const char* key, int from) {
  String needle = String("\"") + key + "\":";
  int i = s.indexOf(needle, from);
  if (i < 0) return -1;
  i += needle.length();
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == '[')) i++;
  return i;
}

bool getNum(const String& s, const char* key, int from, float& out) {
  int i = findValue(s, key, from);
  if (i < 0) return false;
  out = atof(s.c_str() + i);
  return true;
}

bool getStr(const String& s, const char* key, int from, char* out, size_t n) {
  int i = findValue(s, key, from);
  if (i < 0 || s[i] != '"') return false;
  i++;
  size_t k = 0;
  while (i < (int)s.length() && s[i] != '"' && k < n - 1) {
    unsigned char c = s[i];
    if (c == '\\') { i += 2; continue; }
    if (c >= 32 && c <= 126) out[k++] = c;   // keep plain ASCII only
    i++;
  }
  out[k] = 0;
  return true;
}

// =====================================================
//                 NETWORK TASKS
// =====================================================

void startClock(long offsetSec) {
  configTime(offsetSec, 0, "pool.ntp.org", "time.google.com");
  unsigned long start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < 5000) {
    delay(100);
  }
}

bool fetchLocation() {

  if (USE_MANUAL_LOCATION) {
    snprintf(gCity, sizeof(gCity), "%s", MANUAL_CITY);
    snprintf(gRegion, sizeof(gRegion), "%s", MANUAL_REGION);
    snprintf(gCountry, sizeof(gCountry), "%s", MANUAL_COUNTRY);
    gLat = MANUAL_LAT;
    gLon = MANUAL_LON;
    gUtcOffset = MANUAL_UTC_OFFSET_S;
    startClock(gUtcOffset);
    haveLocation = true;
    return true;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(8000);

  if (!http.begin(client,
      "http://ip-api.com/json/?fields=status,message,country,regionName,city,lat,lon,offset")) {
    setErr("Request failed");
    return false;
  }

  int code = http.GET();

  if (code != 200) {
    if (code < 0) setErr("No response");
    else snprintf(gErr, sizeof(gErr), "HTTP error %d", code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  char status[12] = "";
  if (!getStr(body, "status", 0, status, sizeof(status)) ||
      strcmp(status, "success") != 0) {
    setErr("Lookup rejected");
    return false;
  }

  float lat, lon, off;
  if (!getNum(body, "lat", 0, lat) ||
      !getNum(body, "lon", 0, lon) ||
      !getNum(body, "offset", 0, off)) {
    setErr("Bad location data");
    return false;
  }

  getStr(body, "city", 0, gCity, sizeof(gCity));
  getStr(body, "regionName", 0, gRegion, sizeof(gRegion));
  getStr(body, "country", 0, gCountry, sizeof(gCountry));

  if (gCity[0] == 0) snprintf(gCity, sizeof(gCity), "Unknown area");

  gLat = lat;
  gLon = lon;
  gUtcOffset = (long)off;

  startClock(gUtcOffset);

  haveLocation = true;
  return true;
}

bool fetchWeather() {

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(10000);

  char url[440];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
    "is_day,weather_code,wind_speed_10m"
    "&daily=temperature_2m_max,temperature_2m_min"
    "&forecast_days=1&timezone=auto%s",
    gLat, gLon,
    USE_FAHRENHEIT ? "&temperature_unit=fahrenheit&wind_speed_unit=mph" : "");

  if (!http.begin(client, url)) {
    setErr("Request failed");
    return false;
  }

  int code = http.GET();

  if (code != 200) {
    if (code < 0) setErr("No response");
    else snprintf(gErr, sizeof(gErr), "HTTP error %d", code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  int c = body.indexOf("\"current\":{");
  int d = body.indexOf("\"daily\":{");

  if (c < 0 || d < 0) {
    setErr("Bad weather data");
    return false;
  }

  float t, f, h, w, code2, day, hi, lo;

  if (!getNum(body, "temperature_2m", c, t) ||
      !getNum(body, "apparent_temperature", c, f) ||
      !getNum(body, "relative_humidity_2m", c, h) ||
      !getNum(body, "wind_speed_10m", c, w) ||
      !getNum(body, "weather_code", c, code2) ||
      !getNum(body, "is_day", c, day) ||
      !getNum(body, "temperature_2m_max", d, hi) ||
      !getNum(body, "temperature_2m_min", d, lo)) {
    setErr("Missing fields");
    return false;
  }

  wTemp = t;
  wFeels = f;
  wHum = (int)lroundf(h);
  wWind = w;
  wCode = (int)code2;
  wIsDay = day > 0.5f;
  wHi = hi;
  wLo = lo;

  haveWeather = true;
  lastWeatherOk = millis();
  return true;
}

// ---------- run a network job in the background ----------
// (this keeps the animations moving while we wait for the internet)

static NetFn gFn = nullptr;
static volatile bool gDone = true;
static volatile bool gResult = false;

void netTask(void*) {
  gResult = gFn();
  gDone = true;
  vTaskDelete(NULL);
}

void startNetTask(NetFn fn) {
  gFn = fn;
  gDone = false;
  gResult = false;

  if (xTaskCreate(netTask, "net", 12288, NULL, 1, NULL) != pdPASS) {
    gResult = fn();      // fallback: run directly
    gDone = true;
  }
}

// =====================================================
//                 DRAWING PRIMITIVES
// =====================================================

void drawArc(int cx, int cy, int r, int a0, int a1) {
  for (int rr = r; rr <= r + 1; rr++) {
    for (int a = a0; a <= a1; a++) {
      float rad = a * DEG_TO_RAD;
      display.drawPixel(
        cx + (int)lroundf(rr * sin(rad)),
        cy - (int)lroundf(rr * cos(rad)),
        WHITE
      );
    }
  }
}

void drawWifiBig(int cx, int cy, int level) {
  display.fillCircle(cx, cy, 2, WHITE);
  if (level >= 1) drawArc(cx, cy, 8, -45, 45);
  if (level >= 2) drawArc(cx, cy, 15, -45, 45);
  if (level >= 3) drawArc(cx, cy, 22, -45, 45);
}

void drawBadge(int x, int y, bool ok) {

  display.fillCircle(x, y, 8, WHITE);

  if (ok) {
    for (int dy = 0; dy < 2; dy++) {
      display.drawLine(x - 4, y + dy, x - 1, y + 3 + dy, BLACK);
      display.drawLine(x - 1, y + 3 + dy, x + 4, y - 3 + dy, BLACK);
    }
  } else {
    for (int dx = 0; dx < 2; dx++) {
      display.drawLine(x - 3 + dx, y - 3, x + 3 + dx, y + 3, BLACK);
      display.drawLine(x + 3 + dx, y - 3, x - 3 + dx, y + 3, BLACK);
    }
  }
}

void drawBigCheck(int cx, int cy) {
  display.fillCircle(cx, cy, 15, WHITE);
  for (int dy = -1; dy <= 1; dy++) {
    display.drawLine(cx - 8, cy + dy, cx - 3, cy + 6 + dy, BLACK);
    display.drawLine(cx - 3, cy + 6 + dy, cx + 9, cy - 7 + dy, BLACK);
  }
}

void drawSun(int cx, int cy, int r, int tick) {
  display.fillCircle(cx, cy, r, WHITE);
  for (int i = 0; i < 8; i++) {
    float a = (i * 45 + tick * 4) * DEG_TO_RAD;
    display.drawLine(
      cx + (int)(cos(a) * (r + 3)), cy + (int)(sin(a) * (r + 3)),
      cx + (int)(cos(a) * (r + 6)), cy + (int)(sin(a) * (r + 6)),
      WHITE
    );
  }
}

void drawMoon(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, WHITE);
  display.fillCircle(cx + r / 2, cy - r / 3, r - 1, BLACK);
}

void cloudShape(int cx, int cy, int g, uint16_t c) {
  display.fillCircle(cx - 8, cy + 2, 6 + g, c);
  display.fillCircle(cx,     cy - 3, 8 + g, c);
  display.fillCircle(cx + 8, cy + 2, 6 + g, c);
  display.fillRect(cx - 8, cy + 2, 17, 7 + g, c);
}

// white cloud with a black outline so it sits cleanly in front of things
void drawCloud(int cx, int cy) {
  cloudShape(cx, cy, 2, BLACK);
  cloudShape(cx, cy, 0, WHITE);
}

// =====================================================
//               STATUS SCREENS (WiFi etc.)
// =====================================================

void drawStatus(
  StatusIcon icon,
  const char* title,
  const char* l1,
  const char* l2,
  int step,
  int tick,
  const char* label,
  bool pips
) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  char lb[24];
  if (label) snprintf(lb, sizeof(lb), "%s", label);
  else snprintf(lb, sizeof(lb), "STEP %d/4", step);

  display.setCursor(0, 0);
  display.print(lb);

  if (pips) {
    for (int i = 0; i < 4; i++) {
      int x = 96 + i * 8;
      bool solid = (i < step - 1) ||
                   (i == step - 1 &&
                    (icon == ICON_DONE || icon == ICON_WIFI_OK ||
                     ((tick / 5) % 2) == 0));
      if (solid) display.fillRect(x, 1, 6, 4, WHITE);
      else display.drawRect(x, 1, 6, 4, WHITE);
    }
  }

  switch (icon) {

    case ICON_SCAN: {
      // radar sweep
      int cx = 64, cy = 19;
      display.drawCircle(cx, cy, 15, WHITE);
      display.drawCircle(cx, cy, 8, WHITE);
      for (int k = 0; k < 3; k++) {
        float a = (tick * 10 - k * 9) * DEG_TO_RAD;
        display.drawLine(cx, cy,
          cx + (int)(cos(a) * 15), cy + (int)(sin(a) * 15), WHITE);
      }
      if (((tick + 0) % 36) < 20) display.fillCircle(72, 13, 1, WHITE);
      if (((tick + 12) % 36) < 20) display.fillCircle(55, 24, 1, WHITE);
      if (((tick + 24) % 36) < 20) display.fillCircle(70, 26, 1, WHITE);
      break;
    }

    case ICON_CONNECT:
      drawWifiBig(64, 30, (tick / 4) % 4);
      break;

    case ICON_WIFI_OK:
      drawWifiBig(52, 30, 3);
      drawBadge(84, 24, true);
      break;

    case ICON_DONE:
      drawBigCheck(64, 19);
      break;

    case ICON_FAIL:
      drawWifiBig(52, 30, (tick / 6) % 2);
      drawBadge(84, 24, false);
      break;

    case ICON_LOCATE: {
      int bounce = abs((tick % 12) - 6) / 2;
      int hy = 14 - bounce;
      display.fillCircle(64, hy, 8, WHITE);
      display.fillTriangle(57, hy + 5, 71, hy + 5, 64, hy + 17, WHITE);
      display.fillCircle(64, hy, 3, BLACK);
      int w = 9 - bounce * 2;
      display.drawFastHLine(64 - w, 33, 2 * w + 1, WHITE);
      break;
    }

    case ICON_WEATHER: {
      drawCloud(64, 13);
      for (int k = 0; k < 3; k++) {
        int x = 54 + k * 10;
        int y = 22 + ((tick + k * 3) % 8) / 2;
        display.drawFastVLine(x, y, 5, WHITE);
        display.fillTriangle(x - 3, y + 5, x + 3, y + 5, x, y + 9, WHITE);
      }
      break;
    }
  }

  centerText(title, 36);
  centerText(l1, 46);
  centerText(l2, 56);

  display.display();
}

void resultScreen(
  StatusIcon icon, const char* title,
  const char* l1, const char* l2, int step, unsigned long ms
) {
  unsigned long start = millis();
  int t = 0;
  while (millis() - start < ms) {
    drawStatus(icon, title, l1, l2, step, t++, nullptr, true);
    delay(60);
  }
}

void failScreen(int step, const char* title, const char* l1, const char* l2) {

  beepFail();

  unsigned long start = millis();
  int t = 0;

  while (millis() - start < RETRY_MS) {

    unsigned long left = (RETRY_MS - (millis() - start) + 999) / 1000;
    char label[24];
    snprintf(label, sizeof(label), "RETRY IN %lus (TOUCH)", left);

    drawStatus(ICON_FAIL, title, l1, l2, step, t++, label, false);

    if (isPressed(PIN_JUMP)) {
      beepTap();
      while (isPressed(PIN_JUMP)) delay(10);
      break;
    }

    delay(80);
  }
}

// run a network job with an animated screen until it finishes
bool runAnimated(
  NetFn fn, StatusIcon icon, const char* title,
  const char* l1, const char* l2, int step
) {
  startNetTask(fn);
  int t = 0;
  while (!gDone || t < 15) {
    drawStatus(icon, title, l1, l2, step, t++, nullptr, true);
    delay(60);
  }
  return gResult;
}

// =====================================================
//                    BOOT STEPS
// =====================================================

bool stepScan() {

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  WiFi.scanNetworks(true);

  char l1[24], l2[24];
  snprintf(l1, sizeof(l1), "Looking for networks");
  snprintf(l2, sizeof(l2), "Target: %.13s", WIFI_SSID);

  unsigned long start = millis();
  int t = 0;
  int n;

  while (true) {
    n = WiFi.scanComplete();
    unsigned long el = millis() - start;
    if (n != WIFI_SCAN_RUNNING && el > 1800) break;
    if (el > 15000) { n = WIFI_SCAN_FAILED; break; }
    drawStatus(ICON_SCAN, "SCANNING WIFI", l1, l2, 1, t++, nullptr, true);
    delay(60);
  }

  if (n < 0) {
    WiFi.scanDelete();
    failScreen(1, "SCAN FAILED", "WiFi radio error", "Restart the board");
    return false;
  }

  bool found = false;
  int rssi = 0;

  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      found = true;
      rssi = WiFi.RSSI(i);
      break;
    }
  }

  WiFi.scanDelete();

  if (!found) {
    snprintf(l1, sizeof(l1), "Found %d networks", n);
    failScreen(1, "NETWORK NOT FOUND", l1, "Check name & 2.4GHz");
    return false;
  }

  snprintf(l1, sizeof(l1), "Found: %.14s", WIFI_SSID);
  snprintf(l2, sizeof(l2), "Signal: %d dBm", rssi);
  beepOk();
  resultScreen(ICON_DONE, "NETWORK FOUND", l1, l2, 1, 1400);
  return true;
}

bool stepConnect() {

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  int t = 0;
  const char* reason = nullptr;
  char l1[24], l2[24];

  while (true) {

    wl_status_t st = WiFi.status();
    unsigned long el = millis() - start;

    if (st == WL_CONNECTED) break;

    if (st == WL_CONNECT_FAILED && el > 1500) {
      reason = "Wrong password?";
      break;
    }

    if (st == WL_NO_SSID_AVAIL && el > 4000) {
      reason = "Network not reachable";
      break;
    }

    if (el > 20000) {
      reason = "Timed out (20 s)";
      break;
    }

    snprintf(l1, sizeof(l1), "Joining: %.12s", WIFI_SSID);
    snprintf(l2, sizeof(l2), "Waiting... %lus", el / 1000);
    drawStatus(ICON_CONNECT, "CONNECTING", l1, l2, 2, t++, nullptr, true);
    delay(60);
  }

  if (reason) {
    WiFi.disconnect(true);
    failScreen(2, "CONNECTION FAILED", reason, "Check password/signal");
    return false;
  }

  String ip = WiFi.localIP().toString();
  snprintf(l1, sizeof(l1), "%.21s", WIFI_SSID);
  snprintf(l2, sizeof(l2), "IP %s", ip.c_str());
  beepOk();
  resultScreen(ICON_WIFI_OK, "WIFI CONNECTED", l1, l2, 2, 1900);
  return true;
}

bool stepLocation() {

  bool ok = runAnimated(
    fetchLocation, ICON_LOCATE, "FINDING LOCATION",
    "Asking the internet", "for place & time", 3
  );

  if (!ok) {
    failScreen(3, "LOCATION FAILED", gErr, "Is internet working?");
    return false;
  }

  char l1[24], l2[24];
  snprintf(l1, sizeof(l1), "%.21s", gCity);
  snprintf(l2, sizeof(l2), "%.21s", gCountry);
  beepOk();
  resultScreen(ICON_DONE, "LOCATION FOUND", l1, l2, 3, 1700);
  return true;
}

bool stepWeather() {

  bool ok = runAnimated(
    fetchWeather, ICON_WEATHER, "GETTING WEATHER",
    "Downloading forecast", "from open-meteo.com", 4
  );

  if (!ok) {
    failScreen(4, "WEATHER FAILED", gErr, "Server may be busy");
    return false;
  }

  beepOk();
  resultScreen(ICON_DONE, "WEATHER READY", "Data received!", "Starting display...", 4, 1400);
  return true;
}

void bootSequence() {

  while (!(WiFi.status() == WL_CONNECTED && haveLocation && haveWeather)) {

    if (WiFi.status() != WL_CONNECTED) {
      haveLocation = false;   // refresh location after a new connection
      if (!stepScan()) continue;
      if (!stepConnect()) continue;
    }

    if (!haveLocation) {
      if (!stepLocation()) continue;
    }

    if (!haveWeather) {
      if (!stepWeather()) continue;
    }
  }

  nextFetchAt = millis() + WEATHER_REFRESH_MS;
}

// =====================================================
//               WEATHER ICONS (ANIMATED)
// =====================================================

WeatherKind kindFor(int c) {
  if (c == 0 || c == 1) return WX_CLEAR;
  if (c == 2) return WX_PARTLY;
  if (c == 3) return WX_CLOUD;
  if (c == 45 || c == 48) return WX_FOG;
  if (c >= 51 && c <= 67) return WX_RAIN;
  if (c >= 71 && c <= 77) return WX_SNOW;
  if (c >= 80 && c <= 82) return WX_RAIN;
  if (c == 85 || c == 86) return WX_SNOW;
  if (c >= 95) return WX_STORM;
  return WX_CLOUD;
}

const char* descFor(int c, bool day) {
  switch (c) {
    case 0:  return day ? "Clear sky" : "Clear night";
    case 1:  return day ? "Mostly sunny" : "Mostly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: case 48: return "Foggy";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Icy drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: case 73: case 75: return "Snowfall";
    case 77: return "Snow grains";
    case 80: case 81: case 82: return "Rain showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Storm & hail";
    default: return "Unknown";
  }
}

void drawWeatherIcon(WeatherKind k, bool day, int cx, int cy, int tick) {

  switch (k) {

    case WX_CLEAR:
      if (day) {
        drawSun(cx, cy, 9, tick);
      } else {
        drawMoon(cx, cy, 10);
        if ((tick / 6) % 2) {
          display.drawPixel(cx + 14, cy - 9, WHITE);
          display.drawPixel(cx + 11, cy + 9, WHITE);
        } else {
          display.drawPixel(cx + 15, cy - 6, WHITE);
          display.drawPixel(cx - 12, cy - 11, WHITE);
        }
      }
      break;

    case WX_PARTLY:
      if (day) drawSun(cx + 8, cy - 7, 6, tick);
      else drawMoon(cx + 8, cy - 6, 8);
      drawCloud(cx - 3, cy + 3);
      break;

    case WX_CLOUD:
      drawCloud(cx + 6, cy - 6);
      drawCloud(cx - 4, cy + 3);
      break;

    case WX_RAIN:
      drawCloud(cx, cy - 5);
      for (int i = 0; i < 3; i++) {
        int x = cx - 8 + i * 8;
        int y = cy + 6 + ((tick * 2 + i * 5) % 12);
        display.drawLine(x, y, x - 2, y + 3, WHITE);
      }
      break;

    case WX_STORM:
      drawCloud(cx, cy - 5);
      if ((tick % 10) < 6) {
        display.fillTriangle(cx + 2, cy + 4, cx - 4, cy + 16, cx + 1, cy + 16, WHITE);
        display.fillTriangle(cx - 1, cy + 13, cx + 5, cy + 13, cx - 2, cy + 21, WHITE);
      }
      break;

    case WX_SNOW:
      drawCloud(cx, cy - 5);
      for (int i = 0; i < 3; i++) {
        int x = cx - 9 + i * 9 + ((tick / 4 + i) % 2);
        int y = cy + 7 + ((tick + i * 5) % 14);
        display.drawPixel(x, y, WHITE);
        display.drawPixel(x - 1, y, WHITE);
        display.drawPixel(x + 1, y, WHITE);
        display.drawPixel(x, y - 1, WHITE);
        display.drawPixel(x, y + 1, WHITE);
      }
      break;

    case WX_FOG:
      drawCloud(cx, cy - 6);
      for (int i = 0; i < 3; i++) {
        int x = cx - 14 + ((i % 2) ? 3 : 0) + ((tick / 4) % 4);
        display.drawFastHLine(x, cy + 6 + i * 5, 22, WHITE);
      }
      break;
  }
}

// =====================================================
//                  MAIN SCREENS
// =====================================================

int  page = 0;
int  tickMain = 0;

void drawTemp(int x, int y, int val, int size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(val);
  int cx = display.getCursorX();
  if (size == 2) {
    display.drawCircle(cx + 3, y + 2, 2, WHITE);
    display.setCursor(cx + 7, y);
  } else {
    display.drawCircle(cx + 2, y + 1, 1, WHITE);
    display.setCursor(cx + 5, y);
  }
  display.print(TEMP_UNIT);
}

void drawSignal() {
  if (WiFi.status() == WL_CONNECTED) {
    int r = WiFi.RSSI();
    int bars = (r > -55) ? 4 : (r > -65) ? 3 : (r > -75) ? 2 : (r > -85) ? 1 : 0;
    for (int i = 0; i < 4; i++) {
      int h = 2 + i * 2;
      int x = 110 + i * 5;
      if (i < bars) display.fillRect(x, 8 - h, 3, h, WHITE);
      else display.drawFastHLine(x, 7, 3, WHITE);
    }
  } else {
    display.drawLine(112, 1, 118, 7, WHITE);
    display.drawLine(118, 1, 112, 7, WHITE);
  }
}

void drawTopBar(const char* text, bool pin) {

  display.setTextSize(1);
  display.setTextColor(WHITE);

  int x = 0;

  if (pin) {
    display.fillCircle(3, 3, 3, WHITE);
    display.fillTriangle(0, 4, 6, 4, 3, 9, WHITE);
    display.drawPixel(3, 3, BLACK);
    x = 9;
  }

  char b[16];
  snprintf(b, sizeof(b), "%.13s", text);
  display.setCursor(x, 1);
  display.print(b);

  // page dots
  for (int i = 0; i < 3; i++) {
    if (i == page) display.fillRect(92 + i * 6, 3, 3, 3, WHITE);
    else display.drawPixel(93 + i * 6, 4, WHITE);
  }

  // blinking square while refreshing in the background
  if (bgRunning && ((tickMain / 4) % 2)) {
    display.fillRect(104, 2, 3, 3, BLACK);
    display.drawRect(103, 1, 5, 5, WHITE);
  }

  drawSignal();
  display.drawFastHLine(0, 10, SCREEN_W, WHITE);
}

void drawWeatherPage(struct tm& t, bool timeOk) {

  drawTopBar(gCity, true);

  drawWeatherIcon(kindFor(wCode), wIsDay, 20, 31, tickMain);

  // time
  display.setTextSize(2);
  display.setCursor(46, 13);

  char b[16];
  if (!timeOk) {
    snprintf(b, sizeof(b), "--:--");
  } else if (USE_24H_CLOCK) {
    snprintf(b, sizeof(b), "%02d:%02d", t.tm_hour, t.tm_min);
  } else {
    int h = t.tm_hour % 12;
    if (h == 0) h = 12;
    snprintf(b, sizeof(b), "%d:%02d", h, t.tm_min);
  }
  display.print(b);

  int cx = display.getCursorX();
  display.setTextSize(1);

  if (timeOk) {
    display.setCursor(cx + 2, 13);
    display.printf("%02d", t.tm_sec);
    if (!USE_24H_CLOCK) {
      display.setCursor(cx + 2, 22);
      display.print(t.tm_hour >= 12 ? "PM" : "AM");
    }
  }

  // temperature + feels like
  drawTemp(46, 29, (int)lroundf(wTemp), 2);

  display.setTextSize(1);
  display.setCursor(96, 30);
  display.print("FEELS");
  drawTemp(96, 39, (int)lroundf(wFeels), 1);

  // condition
  display.setTextSize(1);
  display.setCursor(46, 45);
  display.print(descFor(wCode, wIsDay));

  // footer
  display.drawFastHLine(0, 54, SCREEN_W, WHITE);

  char f[26];
  snprintf(f, sizeof(f), "HUM %d%%  WIND %.0f%s", wHum, wWind, WIND_UNIT);
  display.setCursor(0, 57);
  display.print(f);
}

void drawDetailsPage(struct tm& t, bool timeOk) {

  char b[28];

  if (timeOk) strftime(b, sizeof(b), "%a %d %b %Y", &t);
  else snprintf(b, sizeof(b), "DETAILS");

  display.setTextSize(1);
  drawTopBar(b, false);

  display.setCursor(0, 13);
  display.printf("%.21s", gRegion[0] ? gRegion : "-");

  display.setCursor(0, 22);
  display.printf("%.21s", gCountry[0] ? gCountry : "-");

  snprintf(b, sizeof(b), "HI %d%c  LO %d%c",
           (int)lroundf(wHi), TEMP_UNIT, (int)lroundf(wLo), TEMP_UNIT);
  display.setCursor(0, 31);
  display.print(b);

  snprintf(b, sizeof(b), "FEELS %d%c  HUM %d%%",
           (int)lroundf(wFeels), TEMP_UNIT, wHum);
  display.setCursor(0, 40);
  display.print(b);

  snprintf(b, sizeof(b), "WIND %.0f %s", wWind, WIND_UNIT);
  display.setCursor(0, 49);
  display.print(b);

  snprintf(b, sizeof(b), "GPS %.2f,%.2f", gLat, gLon);
  display.setCursor(0, 57);
  display.print(b);
}

void drawNetworkPage() {

  drawTopBar("NETWORK", false);

  bool online = (WiFi.status() == WL_CONNECTED);

  drawWifiBig(22, 40, online ? 3 : 0);

  display.setTextSize(1);

  display.setCursor(48, 14);
  if (online) display.printf("%.13s", WiFi.SSID().c_str());
  else display.print("OFFLINE");

  display.setCursor(48, 24);
  if (online) display.printf("%d dBm", WiFi.RSSI());
  else display.print("Reconnecting");

  unsigned long s = millis() / 1000;
  display.setCursor(48, 34);
  display.printf("UP %lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);

  display.setCursor(0, 47);
  if (online) {
    String ip = WiFi.localIP().toString();
    display.printf("IP %s", ip.c_str());
  } else {
    display.print("IP --");
  }

  display.setCursor(0, 57);
  display.printf("Weather: %lum ago", (millis() - lastWeatherOk) / 60000UL);
}

// =====================================================
//                 SPLASH SCREEN
// =====================================================

void splash() {

  tone(PIN_BUZZ, 880, 80);

  for (int t = 0; t < 28; t++) {

    display.clearDisplay();

    drawSun(20, 30, 8, t);

    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(40, 14);
    display.print("WEATHER");
    display.setCursor(40, 31);
    display.print("STATION");

    display.drawRect(25, 48, 78, 5, WHITE);
    display.fillRect(26, 49, (76 * t) / 27, 3, WHITE);

    display.setTextSize(1);
    centerText("XIAO ESP32-C3", 56);

    display.display();
    delay(60);
  }
}

// =====================================================
//                      SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  pinMode(PIN_JUMP, INPUT);   // TTP223B drives the pin itself
  pinMode(PIN_BUZZ, OUTPUT);

  Wire.begin(8, 9);
  Wire.setClock(400000);

  if (!display.begin(0x3C, true)) {
    Serial.println("OLED not found!");
    while (true) delay(1000);
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  splash();

  bootSequence();

  Serial.println("Weather Station Started!");
}

// =====================================================
//                       LOOP
// =====================================================

unsigned long lastFrame = 0;
bool prevTouch = false;
unsigned long lastReconnect = 0;

void loop() {

  unsigned long now = millis();

  if (now - lastFrame < FRAME_MS) {
    return;
  }

  lastFrame = now;
  tickMain++;

  // ----- touch: next page -----

  bool touch = isPressed(PIN_JUMP);

  if (touch && !prevTouch) {
    page = (page + 1) % 3;
    beepTap();
  }

  prevTouch = touch;

  // ----- background weather refresh -----

  if (bgRunning) {

    if (gDone) {
      bgRunning = false;
      nextFetchAt = now + (gResult ? WEATHER_REFRESH_MS : 60000UL);
    }

  } else if (WiFi.status() == WL_CONNECTED &&
             (long)(now - nextFetchAt) >= 0) {

    bgRunning = true;
    startNetTask(fetchWeather);
  }

  // ----- WiFi watchdog -----

  if (WiFi.status() != WL_CONNECTED && now - lastReconnect > 15000) {
    lastReconnect = now;
    WiFi.reconnect();
  }

  // ----- time -----

  struct tm t;
  bool timeOk = getLocalTime(&t, 0);

  // ----- draw -----

  display.clearDisplay();

  if (page == 0) drawWeatherPage(t, timeOk);
  else if (page == 1) drawDetailsPage(t, timeOk);
  else drawNetworkPage();

  display.display();
}
