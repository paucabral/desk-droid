# DeskDroid

*This is a work in progress...*

## Parts and Dependencies

### 💿 Software
1. [Visual Studio Code](https://code.visualstudio.com/)
2. [PlatformIO](https://platformio.org/) (VSCode Extension)

### ⚙️ Hardware

- **Microcontroller**: ESP32-S3 SuperMini

- **Sensors**:
    - TEMT600 (Light Sensor)
    <!-- - MAX 9814 (Sound Sensor) -->
    <!-- - ~~MQ135 (Air Quality Sensor)~~ -->
    <!-- - ~~DHT11 (Temperature and Humidity Sensor) -->
    <!-- - ~~SR501 (Motion Sensor)~~ -->
    - ADXL345 (Accelerometer)
    - TTP223 (Capacitive Touch Sensor)

- **Actuators**:
    - OLED Display 1.3" 128x64
    - Piezo Buzzer (Passive)
    - DC Motor G12 N20 w/ L9110s Motor Driver

- **Power**:
    <!-- - ?? mAh LiPo Battery -->
    <!-- - TP4065 Charger Module -->
    <!-- - DC Boost Step Up Module - 3.7V to 5V -->
    <!-- - Dip Switch - 3 Pins SPDT -->

*Others are to follow..*

## Usage

### 🤖 Schematics
Follow the schematics as seen in [fritzing/desk-droid.fzz](fritzing/desk-droid.fzz). Use [Fritzing](https://fritzing.org/) and follow the instructions at [fritzing/README.md](fritzing/README.md) to import the parts.

### 🌐 Code: Building & Uploading
1. Clone the repository and open in VScode with PlatformIO extension installed.
2. Import the PlatformIO libraries listed in [lib/README.md](lib/README.md).
3. Create a `secrets.ini` file using the [secrets.ini.example](secrets.ini.example) file as reference and place your [OpenWeather API key](https://openweathermap.org/api).
4. Connect the microcontroller and select the proper port.
5. **Build** then **Upload** the project.