# Why?

This project was born from my need to grow tropical bonsai trees in Ontario, Canada during winter. These trees require adequate temperature and humidity. Although they can adapt somewhat to cold weather, they cannot survive the extreme temperatures of -10°C to -20°C typical of this region. Using my knowledge, I decided to create this project.

# How?

### Microcontroller

The system uses an ESP32 microcontroller with Wi-Fi, connected to components like a temperature sensor, relays, and lights. The controller, following parameters defined in the code, manages when to turn on or off a heater, fan, or lights. To avoid system failures (e.g., heater stuck on or lights not turning on), I implemented a logging system. Periodically, temperature and humidity snapshots are taken, packaged as logs, and sent to an API for storage.

### API

The API runs on a Raspberry Pi using a small Flask-based Python program to store logs and expose APIs. This allows an Android app to fetch and display logs on a mobile device.

The system is guided by parameters such as:
- **Time of day**
- **Minimum/maximum temperature**
- **Minimum/maximum humidity**

To mimic natural day/night cycles, the system lowers the minimum required temperature at night and raises it during the day. If the temperature falls below the threshold, the heater turns on. When it rises above the threshold, the heater turns off.

These parameters are constants on the microcontroller but can be remotely updated. The ESP32 makes periodic API requests (heartbeat) to check for changes such as sunrise/sunset times or new temperature limits. Updated parameters are stored in memory but reset to defaults on restart.

### Android App

The Android app serves as the interface for monitoring and managing the greenhouse.

**Key Features:**
- Fetch logs and store them locally (last 24-48 hours).
- Group logs by type and display them clearly for quick insights.
- Configure ESP32 parameters via heartbeat.
- Alarm system: periodically fetch logs and notify the user if values are out of range.
- Generate graphs for temperature, humidity, and light hours, with visual indicators of key events (heater/fan on/off).
