# 🤖 Desk Buddy Project Examples

A collection of interactive embedded systems projects developed as part of the **2026 UWI Robotics Club Project**.

This repository contains different projects that can be uploaded to the Desk Buddy using the Arduino IDE. Each project is provided as an individual Arduino `.ino` file and is designed around the **Seeed Studio XIAO ESP32-C3** microcontroller.

The projects demonstrate different applications of embedded systems, including:
- Internet connectivity and IoT.
- OLED graphics and animations.
- Interactive gaming.
- Capacitive touch sensing.
- State machines and control logic.
- Real-time data processing.

---

# ⚠️ IMPORTANT INFORMATION

All projects in this repository are configured for the:

**Microcontroller:** Seeed Studio XIAO ESP32-C3  
**Display:** 1.3-inch 128 × 64 OLED (SH1106 I2C)

Please ensure that you are using the correct microcontroller and that all components are connected according to the specified pin assignments.

Although other ESP32 development boards may be compatible, their GPIO assignments may differ.

---

# 🔌 1. Standard Hardware Pin Assignments

The following GPIO assignments are shared across the projects unless otherwise specified.

| Component | Component Pin | XIAO ESP32-C3 Pin | GPIO |
|---|---|---|---|
| OLED Display | SDA | D8 | GPIO 8 |
| OLED Display | SCL | D9 | GPIO 9 |
| OLED Display | VCC | 3V3 | 3.3V |
| OLED Display | GND | GND | GND |
| TTP223B Touch Sensor | OUT / I/O | D1 | GPIO 3 |
| TTP223B Touch Sensor | VCC | 3V3 | 3.3V |
| TTP223B Touch Sensor | GND | GND | GND |


### OLED Communication Configuration

The OLED communicates with the ESP32-C3 using the I2C communication protocol.

| Configuration | Value |
|---|---|
| Display Controller | SH1106 |
| Resolution | 128 × 64 pixels |
| I2C Address | 0x3C |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

**Note:** The TTP223B capacitive touch sensor produces a HIGH signal when touched. The projects therefore configure its output pin as `INPUT` and use `HIGH` as the pressed state.

---

# 🌤️ PROJECT 01 – WEATHER STATION

An internet-connected weather station that displays real-time weather information, local time and approximate geographical location using an OLED display.

The ESP32-C3 connects to a wireless network, determines the user's approximate geographical location using their public IP address and retrieves weather information from an online weather API.

## Features

- Automatic Wi-Fi network scanning and connection.
- Animated Wi-Fi connection and status screens.
- Automatic geographical location detection.
- Real-time temperature readings.
- Weather conditions with graphical icons.
- Local time display with AM/PM support.
- Humidity and wind speed readings.
- Daily high and low temperatures.
- Feels-like temperature.
- Wi-Fi signal strength indicator.
- Network information and IP address.
- Automatic weather updates.
- Touch-controlled navigation between display pages.
- Audible feedback using a buzzer.
- Automatic Wi-Fi reconnection.

## Required Components

| Component | Quantity | Description |
|---|---:|---|
| Seeed XIAO ESP32-C3 | 1 | Main microcontroller |
| 1.3-inch SH1106 OLED | 1 | Displays weather information and graphics |
| TTP223B Capacitive Touch Sensor | 1 | Navigates between display pages |
| Passive Buzzer | 1 | Provides audible feedback |
| Jumper Wires | As required | Electrical connections |
| USB-C Cable | 1 | Programming and power |

## Pin Assignments

Uses the standard hardware pin assignments listed above.

## Required Arduino Libraries

| Library | Purpose |
|---|---|
| Adafruit SH110X | SH1106 OLED display driver |
| Adafruit GFX Library | Graphics and drawing functions |
| Adafruit BusIO | Communication support |

The following libraries are included with the ESP32 Arduino framework:

- WiFi.h
- HTTPClient.h
- WiFiClientSecure.h
- Wire.h
- time.h

## External APIs

**IP-API – Location Detection**

https://ip-api.com/

Used to determine approximate geographical location using the public IP address.

**Open-Meteo – Weather Information**

https://open-meteo.com/

Used to retrieve weather information based on geographical coordinates.

No API key is required for the endpoints used in this project.

## Display Navigation

| Page | Information |
|---|---|
| Page 1 – Weather | Local time, temperature, feels-like temperature, weather conditions, humidity and wind speed. |
| Page 2 – Details | Region, country, daily high/low temperatures and geographical coordinates. |
| Page 3 – Network | Wi-Fi status, signal strength, IP address and network information. |

**Controls:**

- Touch the TTP223B sensor to move to the next display page.
- Continue touching to cycle through the available pages.

## Configuration

Before uploading, enter your Wi-Fi credentials into the Arduino sketch.

```cpp
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

**Important:** Never upload your actual Wi-Fi password to a public GitHub repository.

---

# 🦖 PROJECT 02 – DINO RUN

A recreation of Google's famous Chrome Dinosaur Game, designed to run entirely on the 1.3-inch OLED display.

Players control a running dinosaur that must avoid obstacles by jumping over cacti and flying birds. The game becomes progressively more challenging as the player's score increases.

## Features

- Animated running dinosaur with moving legs.
- Jumping mechanics with adjustable gravity.
- Small and large cacti.
- Groups of multiple cacti.
- Flying birds introduced after 200 points.
- Increasing game speed.
- Real-time score tracking.
- High-score tracking.
- Score flashing and sound effects every 100 points.
- Day/night screen inversion every 700 points.
- Scrolling ground decorations.
- Animated clouds.
- Collision detection.
- Game-over screen.
- Touch-controlled restart functionality.

## Required Components

| Component | Quantity | Description |
|---|---:|---|
| Seeed XIAO ESP32-C3 | 1 | Main microcontroller |
| 1.3-inch SH1106 OLED | 1 | Displays the game |
| TTP223B Capacitive Touch Sensor | 1 | Dinosaur jump and game controls |
| Passive Buzzer | 1 | Generates game sound effects |
| Jumper Wires | As required | Electrical connections |
| USB-C Cable | 1 | Programming and power |

## Pin Assignments

Uses the standard hardware pin assignments listed above.

| Component | GPIO | Function |
|---|---:|---|
| OLED SDA | GPIO 8 | I2C data |
| OLED SCL | GPIO 9 | I2C clock |
| TTP223B OUT | GPIO 3 | Jump input |
| Passive Buzzer (+) | GPIO 2 | Sound effects |

## Required Arduino Libraries

| Library | Purpose |
|---|---|
| Adafruit SH110X | SH1106 OLED display driver |
| Adafruit GFX Library | Graphics and drawing functions |
| Adafruit BusIO | Communication support |

Additional libraries included with the ESP32 Arduino framework:

- Wire.h

## Game Controls

| Action | Control |
|---|---|
| Start Game | Touch the TTP223B sensor |
| Jump | Touch the sensor while running |
| Short Jump | Release the sensor early |
| Restart | Touch the sensor after Game Over |

The dinosaur automatically runs throughout the game.

## Game Mechanics

**1. Jumping System**

The dinosaur uses a basic physics-based jumping system.

The jumping behaviour is controlled by:

```cpp
const float JUMP_V = 5.6;
const float GRAVITY = 0.55;
```

Increasing `JUMP_V` increases the initial jump velocity, while increasing `GRAVITY` makes the dinosaur fall faster.

**2. Obstacles**

The game randomly generates different obstacles:

- Single small cactus.
- Two small cacti.
- Three small cacti.
- Single large cactus.
- Two large cacti.
- Low-flying birds.
- High-flying birds.

Birds are introduced after the player reaches 200 points.

**3. Increasing Difficulty**

The game gradually increases its speed based on the player's score.

The maximum game speed is limited to:

```cpp
speed = 8.5f;
```

**4. Scoring System**

The player's score increases as the dinosaur travels further through the game.

Every 100 points:
- The score flashes.
- The buzzer produces an audible notification.

**5. Day/Night Cycle**

The OLED display automatically switches between normal and inverted display modes every 700 points.

**6. High Score**

The game tracks the highest score achieved during the current execution of the program.

Note: The high score is currently stored in RAM and resets when the ESP32-C3 is restarted.

## Customisation Challenges

Students can experiment with the following:

1. Increase or decrease the dinosaur's jump height.
2. Change the gravity and jumping physics.
3. Increase the maximum game speed.
4. Implement permanent high-score storage using Preferences.
5. Add a vibration motor for collision feedback.
6. Introduce additional obstacles and animations.
7. Modify the obstacle spawning frequency.

---

# 😀 PROJECT 03 – EXPRESSIVE DESK BUDDY

An interactive animated face designed to give the Desk Buddy personality through different facial expressions, eye movements and automatic sleep/wake behaviour.

The project uses an OLED display to generate expressive eyes, mouths and additional graphical effects. A capacitive touch sensor allows users to cycle through different moods.

The Desk Buddy also features an automatic sleep system that activates after a period of inactivity.

## Features

### 20 Different Facial Expressions

The Desk Buddy supports the following moods:

| No. | Expression | Description |
|---:|---|---|
| 1 | Neutral | Normal resting expression |
| 2 | Happy | Smiling expression |
| 3 | Sad | Downturned mouth |
| 4 | Angry | Angry eyes and expression |
| 5 | Sleepy | Partially closed eyes |
| 6 | Surprised | Wide-open eyes |
| 7 | Love | Heart-shaped eyes |
| 8 | Wink | Animated one-eye wink |
| 9 | Confused | Uncertain expression |
| 10 | Crying | Animated falling tears |
| 11 | Dizzy | Spiral-shaped eyes |
| 12 | Cool | Sunglasses effect |
| 13 | Excited | Happy expression with stars |
| 14 | Mischievous | Playful expression |
| 15 | Worried | Concerned expression |
| 16 | Bored | Half-closed eyes |
| 17 | Shocked | Exaggerated surprised expression |
| 18 | Laughing | Animated laughing expression |
| 19 | Suspicious | Suspicious-looking eyes |
| 20 | Sleeping | Closed eyes with sleeping symbols |

### Additional Animations

- Automatic blinking.
- Random eye movements.
- Animated wink with gradual eye closing and reopening.
- Animated tears.
- Graphical hearts and stars.
- Different mouth shapes for individual expressions.
- Automatic sleep after inactivity.
- Gradual transition into sleep.
- Gradual wake-up animation.
- Sleeping symbols.
- Touch-controlled interaction.

## Required Components

| Component | Quantity | Description |
|---|---:|---|
| Seeed XIAO ESP32-C3 | 1 | Main microcontroller |
| 1.3-inch SH1106 OLED | 1 | Displays animated facial expressions |
| TTP223B Capacitive Touch Sensor | 1 | Changes facial expressions and wakes the device |
| Jumper Wires | As required | Electrical connections |
| USB-C Cable | 1 | Programming and power |

**Optional Component:**

| Component | Quantity | Description |
|---|---:|---|
| Passive Buzzer | 1 | Available for future audible feedback implementation |

Note: The current expressive-face sketch defines a buzzer pin but does not actively generate sound effects.

## Pin Assignments

| Component | GPIO | Function |
|---|---:|---|
| OLED SDA | GPIO 8 | I2C data |
| OLED SCL | GPIO 9 | I2C clock |
| TTP223B OUT | GPIO 3 | Touch input |
| Buzzer (+) | GPIO 2 | Reserved for future sound functionality |

Uses the standard OLED and touch sensor wiring configuration.

## Required Arduino Libraries

| Library | Purpose |
|---|---|
| Adafruit SH110X | SH1106 OLED display driver |
| Adafruit GFX Library | Graphics and drawing functions |
| Adafruit BusIO | Communication support |

Additional libraries included with the ESP32 Arduino framework:

- Wire.h

## Controls

The TTP223B capacitive touch sensor controls the Desk Buddy's expressions.

| Action | Result |
|---|---|
| Touch sensor once | Changes to the next facial expression |
| Continue touching | Cycles through all 20 expressions |
| Leave untouched | Automatically enters sleep mode after 20 seconds |
| Touch while sleeping | Initiates the wake-up sequence |

The expressions cycle continuously in the following order:

NEUTRAL → HAPPY → SAD → ANGRY → SLEEPY → SURPRISED → LOVE → WINK → CONFUSED → CRYING → DIZZY → COOL → EXCITED → MISCHIEVOUS → WORRIED → BORED → SHOCKED → LAUGHING → SUSPICIOUS → SLEEPING.

After the final expression, the sequence returns to NEUTRAL.

## Automatic Sleep System

The Desk Buddy includes an automatic power-saving-style display behaviour.

When no interaction is detected for 20 seconds, the system initiates a gradual sleep transition.

| Parameter | Duration |
|---|---:|
| Inactivity timeout | 20 seconds |
| Sleep transition | 3.5 seconds |
| Wake-up transition | 1.2 seconds |

### Sleep Sequence

1. The system detects inactivity.
2. The current facial expression begins transitioning.
3. The eyes gradually close.
4. The mouth changes into a relaxed expression.
5. Sleeping symbols gradually appear.
6. The Desk Buddy enters its sleeping state.

### Wake-up Sequence

1. The user touches the capacitive touch sensor.
2. The sleeping animation is interrupted.
3. The eyes gradually reopen.
4. The Desk Buddy returns to its neutral expression.
5. Normal animations resume.

**Note:** This is a software-based sleep animation. The ESP32-C3 remains powered on and continues executing its program; it does not enter hardware deep-sleep mode.

## Customisation Challenges

Students can experiment with:

1. Designing additional facial expressions.
2. Changing the eye size and position.
3. Creating new blinking animations.
4. Implementing different sleep behaviours.
5. Adding sound effects using a buzzer.
6. Introducing a microphone for voice interaction.
7. Connecting the Desk Buddy to an online AI assistant.
8. Adding temperature or environmental sensors.
9. Implementing a hardware sleep mode to reduce power consumption.

---

# 🛠️ 4. SOFTWARE SETUP AND UPLOAD INSTRUCTIONS

All projects are developed using the Arduino programming environment.

## Step 1 – Install Arduino IDE

Download and install the Arduino IDE:

https://www.arduino.cc/en/software

## Step 2 – Install ESP32 Board Support

1. Open Arduino IDE.
2. Navigate to Boards Manager.
3. Search for ESP32.
4. Install the ESP32 board package provided by Espressif Systems.

## Step 3 – Install Required Libraries

Open:

**Sketch → Include Library → Manage Libraries**

Search for and install:

- Adafruit SH110X
- Adafruit GFX Library
- Adafruit BusIO

## Step 4 – Connect the Microcontroller

1. Connect the Seeed XIAO ESP32-C3 using a USB-C cable.
2. Navigate to Tools → Board.
3. Select the appropriate ESP32-C3 board.
4. Select the correct COM port.

## Step 5 – Upload a Project

1. Open the desired `.ino` file.
2. Check that all hardware connections are correct.
3. Verify the code.
4. Upload the program.
5. Wait for the upload to complete.
6. Reset the microcontroller if necessary.

---

# 📁 5. REPOSITORY STRUCTURE

The repository is organised into individual project folders.

```text
Desk_Buddy_Project_Examples/
│
├── README.md
│
├── 01_Weather_Station/
│   └── Weather_Station.ino
│
├── 02_Dino_Run/
│   └── Dino_Run.ino
│
└── 03_Expressive_Desk_Buddy/
    └── Expressive_Desk_Buddy.ino
```

Each project can be downloaded and uploaded independently.

---

# 📚 6. LEARNING OBJECTIVES

These projects are intended to introduce Robotics Club members to practical embedded systems development.

Through these examples, students will gain experience with:

| Topic | Application |
|---|---|
| Microcontrollers | Programming the ESP32-C3 |
| GPIO | Interfacing with external hardware |
| I2C Communication | Communicating with OLED displays |
| Digital Inputs | Reading capacitive touch sensors |
| PWM / Tone Generation | Producing buzzer sound effects |
| Graphics Programming | Drawing shapes and animations |
| State Machines | Managing game states and facial expressions |
| Internet Connectivity | Connecting to Wi-Fi networks |
| API Integration | Retrieving online weather information |
| Timing Functions | Implementing real-time animations |
| Sensors | Interfacing with external electronic components |

---

# 👨‍💻 7. PROJECT INFORMATION

**Project:** Desk Buddy Project Examples  
**Programme:** 2026 UWI Robotics Club  
**Microcontroller:** Seeed Studio XIAO ESP32-C3  
**Programming Environment:** Arduino IDE  
**Programming Language:** C++ (Arduino Framework)  
**Display:** 1.3-inch 128 × 64 SH1106 OLED  
**Input Device:** TTP223B Capacitive Touch Sensor  

---

### 🚀 HAVE FUN EXPERIMENTING!

These projects are intended to serve as starting points for experimentation, creativity and further development.

Students are encouraged to modify the code, introduce new features, experiment with additional sensors and develop their own interactive Desk Buddy applications.

**Learn. Build. Experiment. Innovate.**

Developed as part of the 2026 UWI Robotics Club Project.
