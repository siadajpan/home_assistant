# HA Water Level & Pump Controller

This project implements a Zigbee end-device using ESP-IDF to monitor water level (via a 4-20mA sensor) and control a water pump output.

## Hardware & Connections

* **Board**: ESP32-H2 or compatible Espressif SoC.
* **Water Pump Output**: Connected to **GPIO 4** (`PUMP_GPIO_PIN`). Outputting HIGH (1) turns the pump ON; LOW (0) turns the pump OFF.
* **Water Level Sensor**: Analog sensor connected to **ADC Channel 2** (`ADC_CHANNEL`).
* **Factory Reset Button**: Pressing the BOOT button (connected to **GPIO 9**) triggers a factory reset to re-initiate steering/pairing.
* **Onboard RGB LED**: Displays the state of the device:
  * **Green (RGB: 0, 100, 0)**: Sleep mode disabled, pump is OFF.
  * **Red (RGB: 100, 0, 0)**: Pump is ON.

---

## How to Build the Code

The project is built using the ESP-IDF framework.

1. **Set Up the Environment**:
   Initialize your ESP-IDF environment. On Windows with PowerShell, run the following:
   ```powershell
   . C:\Espressif\Initialize-Idf.ps1 -IdfId esp-idf-1d48cf7427aa464d60cc013d481736ef
   ```
   *(Or activate using your local path to `export.ps1`/`export.bat`)*

2. **Build the Project**:
   ```powershell
   idf.py build
   ```

3. **Flash the Project**:
   Locate your board's COM port (e.g. `COM5`) and flash the code:
   ```powershell
   idf.py -p COM5 flash
   ```

4. **Monitor Output**:
   To view logs and debug output:
   ```powershell
   idf.py -p COM5 monitor
   ```

---

## How to Add to Home Assistant (HASS)

This device communicates via Zigbee and integrates with Home Assistant using **ZHA** or **Zigbee2MQTT**.

1. Put your Zigbee Coordinator/Home Assistant into pairing mode (**Add Device** in ZHA).
2. Power on the device.
3. If the device was previously paired or fails to join, press the **BOOT button (GPIO 9)** on the board. This performs a factory reset, clearing old network info and starting steering/commissioning.
4. Home Assistant will discover the device and create the entities automatically.

---

## Zigbee Endpoints & Switches

The device exposes the following Zigbee endpoints and clusters:

### 1. Water Level Sensor (Endpoint 10)
* **Cluster**: `ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT` (Temperature Measurement)
* **Description**: Although exposed as a temperature sensor to leverage standard Zigbee home automation profiles, the values reported are the **water level in centimeters (cm)**.
* **HASS Entity**: Appears as a Temperature Sensor (e.g. `sensor.espressif_water_level`).

### 2. Water Pump Switch (Endpoint 11)
* **Cluster**: `ESP_ZB_ZCL_CLUSTER_ID_ON_OFF` (On/Off Switch)
* **Description**: Turning this switch ON/OFF from Home Assistant toggles the state of **GPIO 4** (`PUMP_GPIO_PIN`), enabling or disabling the physical water pump.
* **HASS Entity**: Appears as a Switch (e.g. `switch.espressif_pump`).

### 3. Sleep Mode & Responsiveness
* **No Sleep Switch**: The sleep mode toggle switch endpoint (formerly Endpoint 12) has been removed so it **does not show up as a confusing extra switch** in Home Assistant.
* **Disabled by Default**: Sleep mode is disabled by default (`s_sleep_mode_state = false`) to ensure the device remains awake and reacts immediately to command signals from Home Assistant.
