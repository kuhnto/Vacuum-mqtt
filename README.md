Congratulations on reaching this milestone! Since you’ve now got a stable "Gold Master" baseline with GPIO triggers, MQTT integration, and a responsive web portal, your README should reflect the technical depth of the project.

Here is a professionally structured, "rich" README.md template designed specifically for your HA-Vac-Control project. You can copy this directly into your VS Code editor.
HA-Vac-Control (ESP32-C3)

An advanced, MQTT-integrated hardware bridge for robot vacuums. This project allows you to modernize "offline" vacuums by interfacing directly with their button logic using an ESP32-C3 SuperMini, enabling full control via Home Assistant.
🚀 Overview

The software acts as a "Smart Remote," mimicking physical button presses through Open-Drain GPIO triggers. It features a dual-mode networking stack: an Access Point (AP) for initial configuration and a Station (STA) mode for daily operation.
Key Features

    Mobile-First Web Portal: A responsive interface for real-time hardware testing and Wi-Fi/MQTT provisioning.

    Persistent Storage: Uses Non-Volatile Storage (NVS) to remember your settings through power cycles.

    Low-Level Hardware Control: Implements 3.3V Open-Drain logic (GPIO 0, 1, 3) to safely interface with proprietary vacuum logic boards.

    MQTT Event Bus: High-performance, non-blocking MQTT client for integration with Home Assistant vacuum or button entities.

    On-the-Fly Updates: Ability to update MQTT broker settings via the web UI without requiring a full system reboot.

🛠 Hardware Architecture

The software is optimized for the ESP32-C3 SuperMini.
GPIO Mapping
Command	GPIO	Logic Type	Function
DOCK	0	Open-Drain	Pulls "Home" button line to Ground
CLEAN	1	Open-Drain	Pulls "Start/Stop" line to Ground
MAX	3	Open-Drain	Pulls "Turbo" line to Ground
💾 Installation & Flashing
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

3. Initial Configuration

    Power the module. If no Wi-Fi is configured, it will broadcast an AP named ESP32_Vac_Setup.

    Connect to the AP (Password: password123) and navigate to 192.168.4.1.

    Select your home network, enter your MQTT Broker IP (e.g., 192.168.1.50:1883), and Save.

    The device will reboot and join your network.

📡 MQTT API

The device listens on the vacuum/commands topic. Send the following raw strings as payloads:
Payload	Action
DOCK	Triggers GPIO 0 (500ms pulse)
CLEAN	Triggers GPIO 1 (500ms pulse)
MAX	Triggers GPIO 3 (500ms pulse)
📝 Troubleshooting (Linux)

If you encounter a "Port Busy" or "OpenOCD" error while debugging on Linux:

    Ensure ModemManager is disabled: sudo systemctl disable ModemManager.

    Clear stale debugger ports: sudo fuser -k 6666/tcp.

    Ensure your user is in the dialout group: sudo usermod -a -G dialout $USER.