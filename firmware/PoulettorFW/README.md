# Poulettor firmware

ESP32-C3 firmware for the Poulettor automatic chicken coop door controller.

Firmware by Jean-Sébastien Niel.  
GitHub: https://github.com/jsniel

License: GPL-3.0-or-later

---

## Main behavior

- Calculates sunrise and sunset times from hardcoded latitude and longitude.
- Opens the door in the morning using the sunrise time plus an offset.
- Closes the door in the evening using the sunset time plus an offset.
- Stores offsets in EEPROM.
- Drives a DC motor through two relay outputs.
- Stops motion using top and bottom limit switches.
- Stops the motor after a 15-second timeout if no limit switch is reached.
- Provides a local Wi-Fi web interface when requested by button.

---

## Pin mapping

| Function | GPIO |
|---|---:|
| Open / Wi-Fi button | GPIO10 |
| Close button | GPIO21 |
| Status LED | GPIO2 |
| Top limit switch | GPIO3 |
| Motor open relay | GPIO4 |
| Bottom limit switch | GPIO0 |
| Motor close relay | GPIO1 |
| RTC interrupt | GPIO5 |
| SDA | GPIO8 |
| SCL | GPIO9 |

---

## Arduino IDE setup

Add this URL to Arduino IDE Boards Manager:

```txt
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Install:

```txt
esp32 by Espressif Systems
```

Select:

```txt
ESP32C3 Dev Module
```

---

## Required libraries

| Library | Tested version |
|---|---:|
| RTClib by Adafruit | 2.1.4 |
| Adafruit BusIO | 1.17.4 |

---

## Default Wi-Fi portal

```txt
SSID: poulator2000
Password: nounette
IP address: 192.168.4.1
```

---

## Web routes

| Route | Description |
|---|---|
| `/` | Main web page |
| `/ouvrir` | Open the door |
| `/fermer` | Close the door |
| `/setoffset` | Save sunrise/sunset offsets |
| `/settime` | Set RTC time |
| `/datetime` | Return current RTC date and time |
| `/time` | Return current RTC time |
