# Technical Overview

This document summarizes the firmware behavior.

---

## Scheduling

The firmware computes sunrise and sunset times from:

- date from DS3231 RTC;
- hardcoded latitude;
- hardcoded longitude;
- daylight saving time detection.

Morning and evening offsets are stored in EEPROM.

---

## Movement state machine

Movement states:

- `AUCUN`
- `OUVERTURE_EN_COURS`
- `FERMETURE_EN_COURS`

Opening:

- stops any current movement;
- checks the top limit switch;
- starts the open relay;
- waits for top endstop or timeout.

Closing:

- stops any current movement;
- checks the bottom limit switch;
- starts the close relay;
- waits for bottom endstop or timeout.

---

## Timeout protection

If a movement lasts more than 15 seconds, the firmware stops the motor and enters timeout error state.

The LED then blinks rapidly.

---

## Web portal

The web server is only started after a long button press.

It provides:

- manual open;
- manual close;
- offset configuration;
- RTC time setting;
- date/time display;
- countdown before portal shutdown.

---

## Important notes

The location is currently hardcoded:

```cpp
const float LATITUDE = 49.4687;
const float LONGITUDE = 1.1178;
```

Future versions could expose location configuration in the web interface.
