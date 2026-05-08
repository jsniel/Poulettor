# Poulettor User Manual

Poulettor is an automatic chicken coop door controller.

It opens the door in the morning based on sunrise and closes it in the evening based on sunset.

---

## First setup

1. Power the controller.
2. Hold the open/Wi-Fi button for about 3 seconds.
3. Connect to the Wi-Fi network.
4. Open the web interface.
5. Set the RTC time.
6. Adjust the sunrise and sunset offsets if needed.

---

## Wi-Fi portal

```txt
SSID: poulator2000
Password: 123456789
Address: http://192.168.4.1
```

The web portal is started manually with a long press on the open/Wi-Fi button.

---

## Manual control

| Action | Result |
|---|---|
| Short press on open button | Opens the door |
| Short press on close button | Closes the door |
| Long press on open button | Starts Wi-Fi portal |

Manual buttons can interrupt an ongoing movement and reverse direction.

---

## Automatic mode

Poulettor calculates sunrise and sunset times using the date, hardcoded location and daylight saving time.

The user can configure:

- morning offset;
- evening offset.

Example:

```txt
Sunrise: 07:25
Morning offset: +80 min
Door opens: 08:45
```

---

## LED feedback

| LED behavior | Meaning |
|---|---|
| Solid ON | Automatic scheduling available |
| Double blink pattern | Wi-Fi portal active |
| Slow blinking | RTC error |
| Fast blinking | Motor timeout error |

---

## Safety

Check regularly:

- door movement;
- cord condition;
- limit switch position;
- motor mount;
- power supply;
- weather protection.

Never use the system with animals until the mechanism has been tested repeatedly.
