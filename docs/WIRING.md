# Wiring Guide

## ESP32-C3 pin mapping

| GPIO | Function |
|---:|---|
| GPIO10 | Open / Wi-Fi button |
| GPIO21 | Close button |
| GPIO2 | Status LED |
| GPIO3 | Top limit switch |
| GPIO4 | Motor open relay |
| GPIO0 | Bottom limit switch |
| GPIO1 | Motor close relay |
| GPIO5 | RTC interrupt pin |
| GPIO8 | I2C SDA |
| GPIO9 | I2C SCL |

---

## Buttons

Buttons use `INPUT_PULLUP`.

Pressed state is `LOW`.

---

## Limit switches

Limit switches use `INPUT_PULLUP`.

Triggered state is `LOW`.

---

## Motor relays

| Relay | Function |
|---|---|
| Motor open relay | Drives the motor to open the door |
| Motor close relay | Drives the motor to close the door |

The firmware ensures that the opposite relay is turned off before starting a direction.

---

## RTC

DS3231 is connected over I2C:

```txt
SDA: GPIO8
SCL: GPIO9
```
