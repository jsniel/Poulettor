# Hardware

This directory contains the hardware design files for Poulettor.

---

## Files

| Path | Description |
|---|---|
| `fritzing/PoulaillerPorteGND.fzz` | Fritzing project |
| `pcb/gerber/` | Gerber and drill files |
| `bom/BOM.md` | Bill of materials |

---

## Hardware overview

The controller uses an ESP32-C3 Mini, a DS3231 RTC, two relay modules and two limit switches.

One relay drives the motor in the opening direction.  
The other relay drives the motor in the closing direction.

The firmware never intentionally activates both motor directions at the same time.

---

## Limit switches

The two limit switches are wired as inputs with internal pull-ups.

| Limit switch | Function |
|---|---|
| Top endstop | Door fully open |
| Bottom endstop | Door fully closed |

Active state is `LOW`.

---

## Safety

Use a motor and power supply adapted to the mechanical load.

Always test the complete mechanism before installing it in a coop.
