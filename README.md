# ✈️ STM32 Gyro 3D Plane Visualizer

![C](https://img.shields.io/badge/Language-C-blue.svg)
![Python](https://img.shields.io/badge/Language-Python-yellow.svg)
![Platform](https://img.shields.io/badge/Platform-STM32-lightgrey.svg)
![Blender](https://img.shields.io/badge/Tool-Blender_5.0-orange.svg)

A full-stack embedded hardware-to-software pipeline that translates physical motion into a 3D environment in real-time. 

This project reads raw angular velocity from an L3GD20/L3GD20H gyroscope via SPI on an STM32 microcontroller, transmits the parsed data over a Virtual COM Port (UART), and uses a multi-threaded Python script to animate a 3D F-15 aircraft model in Blender with zero perceptible latency.

## 🎥 Demonstration

[![STM32 to Blender Real-Time Demo](https://img.youtube.com/vi/https://youtu.be/NO5X-J4crtQ?si=G93psrgQKab591qn/maxresdefault.jpg)]([https://youtu.be/YOUR_VIDEO_ID](https://youtu.be/[NO5X-J4crtQ?si=G93psrgQKab591qn](https://youtu.be/NO5X-J4crtQ?si=G93psrgQKab591qn)))

*Click the image above to watch the full hardware-in-the-loop demonstration on YouTube.*

## ✨ Technical Highlights
* **Bare-Metal SPI Communication:** Uses STM32 HAL to perform highly efficient "Burst Reads," capturing X, Y, and Z axes simultaneously.
* **Optimized UART Transmission:** Implements fixed-point arithmetic to format and transmit sensor data, intentionally bypassing heavy floating-point `printf` libraries to save MCU flash memory.
* **Real-Time Physics Integration:** Converts raw angular velocity (Degrees Per Second) into absolute Euler angles (Pitch, Roll, Yaw) on the fly.
* **Multi-Threaded Python Engine:** Utilizes a background daemon thread and a Blender Modal Operator to parse live serial data without freezing the 3D rendering UI.
* **Deadband Filtering:** Implements a customizable noise filter to prevent the 3D model from drifting due to inherent sensor noise.

## 🛠️ Hardware & Software Stack
* **Microcontroller:** STM32 Development Board (e.g., STM32F429 / Nucleo / Discovery).
* **Sensor:** L3GD20 / L3GD20H 3-axis Gyroscope (SPI).
* **Languages:** C11 (Embedded), Python 3.11 (Scripting/Integration).
* **3D Environment:** Blender 5.0.

## 🧮 The Math: Velocity to Angle
Gyroscopes measure *angular velocity* ($\omega$), not absolute position. To rotate the 3D plane accurately, the Python script continuously integrates the velocity over time using the following formula:

$$ \theta_{current} = \theta_{previous} + (\omega \times \Delta t) $$

## 🚀 How to Run It

### 1. Embedded Setup (STM32)
1. Open the project in STM32CubeIDE.
2. Compile and flash the firmware to your board.
3. The board will begin transmitting strings via UART in the format: `G,X.XX,Y.YY,Z.ZZ,TEMP\r\n`.

### 2. Software Setup (Blender)
1. Open Blender and ensure your target 3D object is named `Plane` in the Outliner.
2. Navigate to the **Scripting** workspace.
3. Paste the contents of `blender_visualizer.py` into the editor.
4. Update the `SERIAL_PORT` variable in the script to match your STM32's COM port (e.g., `"COM3"`).
5. Click **Run Script** (▶) and physically tilt your STM32 board to control the aircraft!

## 📄 License
This project is open-source and available under the MIT License.
