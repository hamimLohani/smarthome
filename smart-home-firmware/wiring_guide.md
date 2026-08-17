# Smart Home — Wiring Guide

![Wiring Diagram](/Users/Inz_mac/.gemini/antigravity-ide/brain/534ff8fd-5031-4421-b978-2a71900de552/wiring_diagram_1785515322229.png)

---

> [!CAUTION]
> This project connects to **mains AC voltage (220V/110V)** which is **lethal**. Only work on the AC side if you are qualified to do so. Always work with the mains power **unplugged**. Use proper insulated wire, heat-shrink tubing, and an enclosed project box.

---

## 🧰 Parts List

| Component | Spec | Qty |
|---|---|---|
| NodeMCU ESP8266 | v3 (30-pin) | 1 |
| Relay Module | 5V, Active LOW, opto-isolated | 4 |
| Power Supply | Hi-Link HLK-PM01 AC-to-5V, OR 5V USB adapter | 1 |
| Mains cable | 3-core (Live, Neutral, Earth) | as needed |
| AC sockets | Standard wall sockets × 4 | 1 strip |
| Wire | Jumper/hookup wire 22–24 AWG for logic | — |
| AC wire | 1.5mm² rated for your load | — |

---

## 📌 GPIO Pin Map (NodeMCU Silk Labels → GPIO)

| NodeMCU Label | GPIO Number | Connected To |
|---|---|---|
| **D5** | GPIO 14 | Relay 1 IN |
| **D7** | GPIO 13 | Relay 2 IN |
| **D6** | GPIO 12 | Relay 3 IN |
| **D0** | GPIO 16 | Relay 4 IN |
| **D3** | GPIO 0  | Unused |
| **D8** | GPIO 15 | Unused |
| **GND** | Ground | All GNDs |
| **VIN** | 5V in | Relay VCC, Power module out |

---

## ⚡ Power Wiring

```
Option A — Desktop testing:
  USB cable → NodeMCU USB port
  NodeMCU 5V pin (VIN) → Relay VCC × 4
  NodeMCU GND         → Relay GND × 4

Option B — Inside the plug strip (AC powered):
  Mains Live + Neutral → Hi-Link AC-DC 5V module
                         ├─► 5V out → NodeMCU VIN
                         ├─► 5V out → Relay VCC × 4
                         └─► GND   → NodeMCU GND + Relay GND × 4
```

> Do **not** power the relays from 3.3V — they need 5V.

---

## 🔴 Relay Modules (× 4, Active LOW)

Each relay module has two sides:

**Coil side (logic, low voltage):**
```
Relay Pin  →  NodeMCU / Power
──────────────────────────────
VCC        →  5V (VIN)
GND        →  GND
IN         →  GPIO pin (see table above)
```

**Switch side (AC mains — ⚠️ HIGH VOLTAGE):**
```
Relay Terminal  →  Purpose
──────────────────────────────────────────────────
COM             →  Mains LIVE wire (from wall)
NO              →  Socket LIVE terminal
NC              →  Leave disconnected (unused)
```

The **Neutral** wire goes **directly** from the wall to all sockets without passing through the relay.

**Relay → GPIO mapping:**
```
Relay 1  IN ── GPIO14 (D5)   [Safe GPIO, recommended first relay]
Relay 2  IN ── GPIO13 (D7)   [Safe for output]
Relay 3  IN ── GPIO12 (D6)   [Safe for output]
Relay 4  IN ── GPIO16 (D0)   [No PWM, no interrupt — basic only]
```

---

## ⚠️ Critical Notes on Boot Pins

The ESP8266 uses certain GPIO pins to decide how to boot. Getting these wrong will prevent the device from starting:

| Pin | Behaviour on boot | Your setup |
|---|---|---|
| **GPIO0 (D3)** | Must be **HIGH** at power-on to boot normally. If pulled LOW → enters flash mode | Unused. |
| **GPIO2 (D4)** | Must be **HIGH** at power-on. | Unused. Pin stays HIGH ✅ |
| **GPIO15 (D8)** | Must be **LOW** at power-on | Unused. Leave this pin alone for reliable boot. |
| **GPIO1 (TX)** | Serial transmit pin | Unused. |
| **GPIO3 (RX)** | Serial receive pin | Unused. |

> **GPIO15 note:** D8/GPIO15 is intentionally unused because it is a boot strap pin and can cause bad behavior.


---

## 🔌 AC Mains Wiring (Inside the Plug Strip)

```
Wall Plug
   │
   ├── LIVE (brown/red)
   │    └── splits to → Relay 1 COM
   │                  → Relay 2 COM
   │                  → Relay 3 COM
   │                  → Relay 4 COM
   │                  → Hi-Link AC input L
   │
   ├── NEUTRAL (blue)
   │    └── splits to → Socket 1 Neutral directly
   │                  → Socket 2 Neutral directly
   │                  → Socket 3 Neutral directly
   │                  → Socket 4 Neutral directly
   │                  → Hi-Link AC input N
   │
   └── EARTH (green/yellow)
        └── Earth terminal on each socket (safety)

Each Relay NO → Socket 1/2/3/4 LIVE terminal
```

---

## 🔁 Full Logic Summary

```
Relay IN pin = LOW  (0V)  →  Relay coil energises  →  COM–NO connected  →  Socket ON  ✅
Relay IN pin = HIGH (3.3V) → Relay coil de-energises → COM–NO open       →  Socket OFF ✅
```

This is **Active LOW** logic, which is already configured in the firmware:
```cpp
#define RELAY_ACTIVE_LOW true   // in config.h
```

---

## 🧪 Quick Test (Before Connecting AC)

1. Flash the firmware to the NodeMCU
2. Open Serial Monitor at **115200 baud**
3. Power on — you should see boot messages
4. Connect to device via WiFi portal or Web Dashboard
5. Toggle Port 1
6. You should hear the relay **click** — the NO terminal is now connected to COM

Test all 4 relays this way before wiring any mains voltage.

---

## 📦 Suggested Enclosure Layout

```
┌─────────────────────────────────────┐
│                                     │
│  [NodeMCU]   [4x Relay Module]     │
│                                     │
│  [Hi-Link 5V PSU]                  │
│──────────────────────────────────── │
│  [Socket 1] [Socket 2]             │  ← Front face
│  [Socket 3] [Socket 4]             │
└─────────────────────────────────────┘
```
