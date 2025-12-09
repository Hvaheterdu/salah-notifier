# Salah Notifier

The **Salah Notifier** is an Arduino-based device that displays daily prayer times on an LCD screen and plays the adhan (call to prayer) through a speaker when it’s time to pray.

---

## 📖 Overview

This project was created to make it easier to keep track of prayer times without needing to constantly check a phone or website. The device shows both the current time and the next prayer time, and automatically plays the adhan when a prayer begins.  

It’s placed in a central location — such as the kitchen — so the adhan can be heard from most areas of the house.

---

## ⚙️ Hardware Setup

The Arduino is housed in a 3D-printed case designed with a slot for a 16x2 LCD display. The speaker is mounted externally through small openings in the case and secured with tape.

- The **LCD display** and **speaker** were purchased from eBay.  
- In Norway, daily prayer times are retrieved via the [bonnetid.no](https://bonnetid.no) API.  
- Prayer times are updated automatically each day at **01:00 AM**, since they vary from day to day.  
- An **SD card module** stores the adhan as an MP3 file. The adhan plays for approximately **18 seconds** at the start of each prayer time.

The LCD display shows:
- **Top line:** Current date and time  
- **Bottom line:** Next prayer time (alternating every 3 seconds between the end of Asr and the start of Maghrib)

---

## 🧰 Components Used

This project was built using YouTube tutorials, components sourced from eBay and Arduino’s official store, and programmed via the Arduino IDE.

| Component | Description / Link |
|------------|--------------------|
| **Arduino Uno R4 WiFi** | [Arduino Uno R4 WiFi](https://store.arduino.cc/products/uno-r4-wifi) |
| **Case** | [KKSB Arduino UNO R4 Project Case for UNO R4 Minima and UNO R4 WiFi](https://kksb-cases.com/products/kksb-arduino-uno-r4-project-case-for-uno-r4-minima-and-uno-r4-wifi) |
| **16x2 LCD Display** | [LCD Display with I²C Interface](https://store.arduino.cc/products/16x2-lcd-display-with-i-c-interface?queryID=undefined) |
| **DFPlayer Mini** | [DFPlayer Mini MP3 Module](https://store.arduino.cc/products/dfplayer-a-mini-mp3-player?queryID=undefined) |
| **Speaker** | [Speaker (eBay)](https://www.ebay.com/itm/127107431783) |
| **Cables** | [40-Pin Jumper Wire Set](https://store.arduino.cc/products/40-colored-male-female-jumper-wires?queryID=766a826bebf1152b10588fac8a68a7a8) |
| **SD Card** | Any SD card (≤ 32GB), formatted as FAT16 or FAT32 |

---

## :camera: Images

![Front of device](https://github.com/Hvaheterdu/salah-notifier/blob/main/docs/images/device_front.jpeg)
![Back of device](https://github.com/Hvaheterdu/salah-notifier/blob/main/docs/images/device_back.jpeg)
![Top of device](https://github.com/Hvaheterdu/salah-notifier/blob/main/docs/images/device_top.jpeg)

## 🕋 Summary

The Salah Notifier is a compact and practical solution for displaying daily prayer times and automatically playing the adhan. It combines hardware simplicity with smart daily updates from an online API, making it both functional and elegant for any Muslim household.
