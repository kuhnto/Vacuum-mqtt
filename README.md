I found a Shark Robot vacuum at a garage sale about 6 years ago. The vacuum fan was broken, so the owner gave it to me. $20 later and the unit was running again. I get the app to get it setup, and then eventually added it to home assistant when I started getting into home automation. all was great for 4 years.

I recently read somewhere that Shark was pulling their open API or something. I did not follow it much, but I then started getting emails saying to "Log In" to get it working again. I am now getting this every month or so. 

At this point I decided to take matters into my own hands and come up with something Cheap, Simple, and accessible for most people. For the vacuum itself, I only really needed to press a few buttons on the top. Seemed like a simple task for a small ESP module. Everyone else is using them, myself included, but I have never started my own project (I got close sometimes). I wanted this to be simple MQTT activated trigger for the three buttons. I originally wanted zigbee, but I ordered the wrong module. I am not a huge fan if wifi devices, but since I had the module and everyone has wifi, I decided to just deal with it. 

The plan was for three GPIOs set to Open-Drain mode, to ground the three 3.3V signals that are routed to the three micro switch buttons. This wa it was a single wire for each button + Power and ground. I was going to add a sensor for detecting if the unit was actually running but ran out of steam. 

This is my first dive into embedded programming since my days working on a PXA250 with the C# compact Framework. Wow things have changed. Most of the development was done with the help of Google Gemini. It is interesting, as it made it easier in the in the same way an electric drill makes drilling holes easier. You still need to know where to drill.

<img width="880" height="820" alt="Vacuum-mqtt drawio" src="https://github.com/user-attachments/assets/8e202e43-c06a-4a2f-aed6-da841b5b17a2" />


-------------START AI Documentation -------------

An advanced, MQTT-integrated hardware bridge for robot vacuums. This project allows you to modernize "offline" vacuums by interfacing directly with their button logic using an ESP32-C3 SuperMini, enabling full control via Home Assistant.
Overview

The software acts as a "Smart Remote," mimicking physical button presses through Open-Drain GPIO triggers. It features a dual-mode networking stack: an Access Point (AP) for initial configuration and a Station (STA) mode for daily operation.
Key Features

    Mobile-First Web Portal: A responsive interface for real-time hardware testing and Wi-Fi/MQTT provisioning.

    Persistent Storage: Uses Non-Volatile Storage (NVS) to remember your settings through power cycles.

    Low-Level Hardware Control: Implements 3.3V Open-Drain logic (GPIO 0, 1, 3) to safely interface with proprietary vacuum logic boards.

    MQTT Event Bus: High-performance, non-blocking MQTT client for integration with Home Assistant vacuum or button entities.

    On-the-Fly Updates: Ability to update MQTT broker settings via the web UI without requiring a full system reboot.

Hardware Architecture

The software is optimized for the ESP32-C3 SuperMini.
GPIO Mapping
Command	GPIO	Logic Type	Function
DOCK	0	Open-Drain	Pulls "Home" button line to Ground
CLEAN	1	Open-Drain	Pulls "Start/Stop" line to Ground
MAX	3	Open-Drain	Pulls "Turbo" line to Ground
Installation & Flashing
Prerequisites

    ESP-IDF v5.x (Developed and tested on Linux Mint/Ubuntu).

    VS Code with the Espressif Extension.

1. Preparing the Module

Because the SuperMini uses the internal USB-Serial JTAG controller, you may need to force the board into Download Mode:

    Hold the BOOT button.

    Press the RESET button.

    Release the BOOT button.

2. Flashing via Command Line

If you are sharing this with a third party, provide the bootloader.bin, partition-table.bin, and app.bin files. They can flash using esptool:
Bash

esptool.py -p /dev/ttyACM0 -b 460800 --chip esp32c3 write_flash \
  0x0 bootloader.bin \
  0x8000 partition-table.bin \
  0x10000 HA-Vac-Control.bin

  Note: The provided binaries are compiled for the ESP32-C3 architecture. Using these on a standard ESP32 or ESP32-S3 will not work and may require a different partition table.

3. Initial Configuration

    Power the module. If no Wi-Fi is configured, it will broadcast an AP named ESP32_Vac_Setup.

    Connect to the AP (Password: password123) and navigate to 192.168.4.1.

    Select your home network, enter your MQTT Broker IP (e.g., 192.168.1.50:1883), and Save.

    The device will reboot and join your network.

MQTT API

The device listens on the vacuum/commands topic. Send the following raw strings as payloads:
Payload	Action
DOCK	Triggers GPIO 0 (500ms pulse)
CLEAN	Triggers GPIO 1 (500ms pulse)
MAX	Triggers GPIO 3 (500ms pulse)
Troubleshooting (Linux)

If you encounter a "Port Busy" or "OpenOCD" error while debugging on Linux:

    Ensure ModemManager is disabled: sudo systemctl disable ModemManager.

    Clear stale debugger ports: sudo fuser -k 6666/tcp.

    Ensure your user is in the dialout group: sudo usermod -a -G dialout $USER.



For the Homeassistant MQTT settings, you can add a button with MQTT publish on tap.  See below:

    - entity: button
        icon: mdi:robot-vacuum
        name: Vac Clean
        type: custom:button-card
        tap_action:
          action: perform-action
          perform_action: mqtt.publish
          data:
            topic: vacuum/commands
            payload: CLEAN
            qos: 1
            retain: false
        color: blue



## ⚖️ License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.


