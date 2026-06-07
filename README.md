 ***A Multi-Sensor Environmental Monitoring and Closed-Loop Actuation System built on ESP32.***

 

---

## 📌 Project Overview
**AmbientSync-ESP32** is a production-grade, non-blocking embedded system designed to monitor micro-climatic parameters and perform real-time, closed-loop environmental controls. Integrating temperature, humidity, ambient light, and acoustic sensors, the system processes multi-channel analog and digital inputs, dynamically updates a high-refresh SPI TFT display, and operates actuators using hysteresis and adaptive control algorithms.

This project was built to demonstrate academic competence in **cooperative multitasking (non-blocking architecture)**, **analog signal processing in the time domain**, and **robust hardware-software co-design**.

---



## 🛠️ Key Engineering Highlights
*   **Non-Blocking Cooperative Multitasking:** Completely avoids the use of `delay()`, relying instead on `millis()` timers to ensure responsive sensor polling and display updates.
*   **Closed-Loop Hysteresis Control:** The ultrasonic mist maker is controlled via a software-defined hysteresis loop, preventing rapid on/off switching (chattering) around the threshold limits to preserve hardware longevity.
*   **Acoustic Peak-to-Peak Windowed Sampling:** Evaluates true environmental sound pressure levels by sampling the MAX4466 microphone analog signal over a 50ms window to extract the actual amplitude envelope, ignoring raw static offsets.
*   **Adaptive PWM Duty-Cycle Mapping:** Adjusts the intensity of an auxiliary LED using pulse-width modulation (PWM) based on the inverse percentage of ambient light under low-light conditions.

---

## 🔌 Hardware Architecture & Wiring Diagram
The system runs on a 30-pin ESP32 NodeMCU development board, powered by a regulated 5V power supply, with a common-ground architecture.

| Peripheral | Component Pin | ESP32 Pin | Interface Type | Technical Description |
| :--- | :--- | :--- | :--- | :--- |
| **ST7789 TFT** | VCC | 3V3 | Power | Regulated 3.3V Line |
| | GND | GND | Power | Common Ground |
| | CS | GPIO5 | Hardware SPI | Chip Select |
| | RESET | GPIO4 | GPIO | Hardware Reset Pulse |
| | DC/RS | GPIO2 | GPIO | Data / Command Selection |
| | SDI (MOSI) | GPIO23 | Hardware SPI | Master Out Slave In |
| | SCK | GPIO18 | Hardware SPI | Serial Clock |
| | LED | 3V3 | Power | Backlight Constant Power |
| **DHT11** | VCC | 3V3 | Power | 3.3V Power Line |
| (3-Pin Module)| DATA | GPIO27 | Single-Wire Digital | Pulled-up DHT11 Data Stream |
| | GND | GND | Power | Common Ground |
| **LDR Circuit** | Analog Out | GPIO34 | Analog (ADC1_CH6) | Voltage Divider (LDR + 10kΩ) |
| **MAX4466 Mic**| OUT | GPIO35 | Analog (ADC1_CH7) | AC Audio Waveform Input |
| **Mist Actuator**| Base Resistor | GPIO12 | GPIO Output | Pulses 2N2222 base for button press |
| **Tactile Switch**| Pin 1 | GPIO13 | GPIO Input | Active LOW, configured with internal pull-up |
| **Red Alert LED**| Anode | GPIO14 | GPIO Output | High-temperature flashing warning indicator |
| **Green LED** | Anode | GPIO25 | PWM Output | Adaptive brightness control |

## 🖼️ Built Circuit Picture
Below is the actual physical hardware implementation, illustrating the layout of the ESP32 controller, ST7789 TFT display, DHT11 sensor, and the custom 2N2222-based driver circuit assembled on the breadboard.

<p align="center">
  <img width="1280" height="960" alt="photo_2026-06-07_03-42-30" src="https://github.com/user-attachments/assets/a4b97684-8e38-46cc-ae95-fca2b041a946" />
</p>

---

## 🎬 Real-Time Live Test

### 1. Temperature Alert & Hazard Indicator 🔴
When the environmental temperature exceeds **32°C**, the system triggers a warning state: the Red LED blinks continuously using an asynchronous timing loop, and a visual warning flag `[ ! ]` appears next to the temperature reading on the TFT.


https://github.com/user-attachments/assets/47a9c734-a211-4e1d-ac0c-b1a9ff9c2856



### 2. Humidity Regulation 💧
When humidity drops below **40%**, the system initiates a single electronic pulse simulating a button-press on the humidifier board to turn on the mist. Once humidity crosses the **50%** threshold, it pulses the board twice to cycle it safely to the OFF state.


https://github.com/user-attachments/assets/04be6fb1-9d75-4691-b4b1-3071a964129a





### 3. Adaptive LED 🟢
Using the hardware-based PWM channel of the ESP32, the LED activates when light levels fall below 40%. The duty cycle scales inversely with the ambient light level—achieving maximum brightness at absolute darkness (0%).


https://github.com/user-attachments/assets/6548c126-0dd1-4589-b47a-9d49d818928d



### 4. Environment Sound Analyze 🎙️
The microphone samples the environment to calculate the peak-to-peak amplitude. Based on empirical thresholds, it classifies the room noise level into 5 distinct categories: *Quiet*, *Moderate*, *Normal*, *Loud*, and *Very Loud*, displaying the results dynamically in color-coded text.


https://github.com/user-attachments/assets/4a4bd160-84fd-4ab0-bfd3-4722663feb33


---

## 💻 How to Build & Run
1.  **Clone the Repository:**
    ```bash
    git clone https://github.com/SepSR/A-Multi-Sensor-Environmental-Monitoring-built-on-ESP32.git
    ```
2.  **Required Libraries:** Install the following libraries via Arduino IDE Library Manager:
    *   `Adafruit GFX Library`
    *   `Adafruit ST7735 and ST7789 Library`
    *   `DHT sensor library` by Adafruit
3.  **Board Configuration:** 
    *   Select **ESP32 Dev Module** in Arduino IDE.
    *   Set **Partition Scheme** to *Default 4MB with SPIFFS*.
4.  **Upload:** Connect your ESP32 via USB and upload the sketch.


---

## 🎓 Future Research & Engineering Directions
To transition this prototype into a research-grade environmental monitor, the following engineering and academic extensions are proposed:

### 1. Edge-AI and TinyML Integration
*   **Acoustic Event Detection (AED):** Replace the current threshold-based peak-to-peak sound analysis with a lightweight neural network (using *TensorFlow Lite for Microcontrollers*). This would enable the system to classify specific ambient sounds (e.g., human speech, alarm sirens, glass breaking, or HVAC anomalies) rather than just measuring raw amplitude.
*   **Predictive Maintenance:** Implement anomaly detection algorithms locally on the ESP32 to predict humidifier failure modes by analyzing changes in current consumption and vibration patterns of the ultrasonic disc.

### 2. Advanced Control Systems
*   **Proportional-Integral-Derivative (PID) Control:** Upgrade the current binary hysteresis on/off control to a closed-loop PID algorithm. By regulating a variable-voltage buck converter or utilizing PWM on the mist maker, the system can dynamically adjust the rate of humidification, minimizing overshoot and maintaining a perfectly stable target humidity.
*   **Multi-Variable Fuzzy Logic:** Implement a fuzzy logic controller that coordinates both temperature and humidity simultaneously to optimize the "heat index" or "apparent temperature" of the local environment rather than treating them as isolated variables.

### 3. Digital Twin & Cloud Telemetry
*   **MQTT & TimescaleDB Pipeline:** Establish an asynchronous MQTT client on the ESP32 to stream telemetry to an InfluxDB or TimescaleDB database. 
*   **Grafana Visualization:** Build a cloud-hosted Grafana dashboard to track historical climatic trends, perform long-term sensor calibration, and run predictive analytics.

### 4. Low-Power Optimization & Energy Harvesting 
*   **Deep Sleep & ULP Co-processor:** Leverage the ESP32's Ultra-Low-Power (ULP) co-processor to sample sensors while the main dual-core processor is in deep sleep. This would reduce the system's idle power consumption from ~100mA to less than 15µA.
*   **Solar Energy Harvesting:** Design a maximum power point tracking (MPPT) battery charger circuit to enable complete off-grid autonomy using small PV panels and LiFePO4 batteries.
