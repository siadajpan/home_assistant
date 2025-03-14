## Overview
The system is design to measure the water level height 0-1m and send the measurement 
to the Home Assistant through zigbee.
This project is based on the temperature sensor from https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_temperature_sensor
That is why it's sending the temperature instead of water level, because I'm stupid 
and I didn't know how to change it. 
The temperature in Celsius is really the water level in cm.

### Hardware
Board is the esp32h2.

Water level sensor that is outputting 4-20mA for the water level 0-1m.
Sensor: https://www.amazon.com.be/dp/B08BC2HG14 with description "0-1M Liquid Level Sensor, 12-32V DC 4-20mA, Oil Water Level Sensor, Accuracy 0.2% FS-0.5% FS, Cable 5M, -40-70°C"

Current to voltage is converted using 130 Ohm resistor. 
Water level sensor connected to its ADC_CHANNEL_2.
