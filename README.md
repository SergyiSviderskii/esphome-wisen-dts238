# esphome-wisen-dts238

markdown
# ESPHome Integration for DTS-238-7 Three-Phase Smart Meter (WISEN / Hekr Protocol)

Local ESPHome firmware integration for the DTS-238-7 three-phase smart meter (WISEN / Hekr protocol). Provides complete cloud-free Home Assistant integration via API/MQTT with physical button fail-safe support.

---

A custom firmware setup to integrate the **DTS-238-7** three-phase electric energy meter into **Home Assistant** completely locally via **ESPHome**, bypassing the cloud. This hardware natively uses the WISEN application and communicates over the Hekr protocol.

### 💡 Project Overview
* **Hardware:** DTS-238-7 Three-Phase Rail Energy Meter
* **Protocol:** Hekr Protocol ([Official Reference Manual](http://hekr.me))
* **Integration Engine:** ESPHome (Local API or MQTT)
* **Control Layer:** Complete feature parity with factory firmware, exposing active energy metrics, voltage, current, calibrations, and remote switching entities.

### ⚠️ Fail-Safe Architecture Note
If the smart meter is turned **OFF** via software (the app/Home Assistant) and the main house power is lost, your Wi-Fi router will drop offline (if the meter acts as the main grid entry point). **You can always turn the meter back ON physically by pressing the top hardware button on the device itself.** The physical button remains completely operational as a fail-safe override.

---

### 🛠️ Step-by-Step Installation Guide

#### 1. Hardware Modification
1. Disassemble the DTS-238-7 meter case.
2. Carefully unsolder the internal ESP Wi-Fi module from the main board.
3. Connect the unsoldered ESP module to your USB-UART programmer (GND, 3.3V, TX, RX).

#### 2. Firmware Configuration & Flashing
1. Open your **ESPHome Dashboard** in Home Assistant.
2. Create a new device configuration file using the built-in wizard or open your existing setup.
3. Integrate this project as an **external component** by adding the following block to your configuration YAML:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com
    components: [ smdst2387 ]

# Add your sensor and configuration nodes using the component's platforms below
```
4. If you prefer a standalone deployment, you can use the complete reference templates provided in this repository.

#### 3. Reassembly & Deployment
1. Solder the flashed ESP module back onto the meter's PCB.
2. Reassemble the plastic housing.
3. Mount the meter and turn on the main power. The device will automatically connect to your network.

#### 4. Dashboard Visualization
* Use the provided `lovelace.yaml` file to instantly build a comprehensive visualization dashboard card for Home Assistant.
---
