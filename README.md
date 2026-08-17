# Smart Home — Complete System Manual

**Firmware Version:** v1.6.0  
**Last Updated:** August 2026

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Hardware Specifications](#2-hardware-specifications)
3. [Hardware Variants](#3-hardware-variants)
4. [Wiring & Assembly](#4-wiring--assembly)
5. [Firmware Architecture](#5-firmware-architecture)
6. [WiFi Setup & Recovery](#7-wifi-setup--recovery)
7. [MQTT Communication Protocol](#8-mqtt-communication-protocol)
8. [Timer System](#9-timer-system)
9. [Schedule System](#10-schedule-system)
10. [Web Dashboard (React App)](#11-web-dashboard-react-app)
11. [Device Pairing & Family Sharing](#12-device-pairing--family-sharing)
12. [Firebase Backend](#13-firebase-backend)
13. [Factory Provisioning & Flashing](#14-factory-provisioning--flashing)
14. [Security Architecture](#15-security-architecture)
15. [Troubleshooting Guide](#16-troubleshooting-guide)
15. [Known Limitations](#17-known-limitations)

---

## 1. System Overview

The **Smart Home** is a WiFi-controlled smart power strip powered by the ESP8266 (NodeMCU) microcontroller. It allows users to independently control 2, 3, or 4 AC outlets via:

- **Web dashboard** (React PWA) accessible from any browser or installed as a mobile app
- **MQTT protocol** for real-time bidirectional communication between device and web app

### Key Capabilities

| Feature | Description |
|---|---|
| **Remote Control** | Toggle individual outlets ON/OFF from anywhere via the web dashboard |
| **Delay Timers** | One-off countdown timers (1 min – 999 min) that work offline (no WiFi needed) |
| **Scheduled Timers** | Clock-based recurring ON/OFF schedules (requires WiFi for NTP time sync) |
| **Multi-Slot Scheduling** | Up to 4 independent schedule slots per outlet |
| **Multi-Device Support** | One account can control multiple Smart Homes |
| **Family Sharing** | Invite family members to control shared plugs with owner approval |
| **Bilingual UI** | Web dashboard supports English and Bangla (বাংলা) |
| **PWA Support** | Install as a native-like app on mobile devices |
| **Offline Operation** | Relays retain state and buttons work even without WiFi |
| **Relay State Persistence** | Outlet ON/OFF states survive power loss and reboot |
| **Live Online/Offline Status** | Real-time device connectivity indicator with active sync pings |

### System Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                       USER DEVICES                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                      │
│  │ Phone    │  │ Tablet   │  │ Desktop  │                      │
│  │ (PWA)    │  │ (Browser)│  │ (Browser)│                      │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                      │
│       │              │              │                            │
│       └──────────────┼──────────────┘                            │
│                      │                                           │
│              ┌───────▼───────┐                                   │
│              │  React Web    │                                   │
│              │  Dashboard    │                                   │
│              │  (Vite PWA)   │                                   │
│              └──┬─────────┬──┘                                   │
│                 │         │                                      │
│     ┌───────────▼──┐  ┌──▼──────────┐                           │
│     │   Firebase   │  │ HiveMQ MQTT │                           │
│     │  Auth + RTDB │  │   Broker    │                           │
│     │  + Functions │  │ (TLS/WSS)   │                           │
│     └──────────────┘  └──────┬──────┘                           │
│                              │                                   │
│                    ┌─────────▼─────────┐                        │
│                    │   ESP8266 Firmware │                        │
│                    │  (Smart Home)│                        │
│                    │        ┌──────┐  │                        │
│                    │        │Relays│  │                        │
│                    │        └──────┘  │                        │
│                    │  ┌────────────┐   │                        │
│                    │  │ 3 Buttons  │   │                        │
│                    │  └────────────┘   │                        │
│                    └───────────────────┘                        │
└──────────────────────────────────────────────────────────────────┘
```

---

## 2. Hardware Specifications

### Components List

| Component | Specification | Qty |
|---|---|---|
| NodeMCU ESP8266 | v3 (30-pin), ESH-12E/F | 1 |
| Relay Module | 5V, Active LOW, opto-isolated | 2–4 |
| Power Supply | Hi-Link HLK-PM01 AC-to-5V, or 5V USB adapter | 1 |
| Mains cable | 3-core (Live, Neutral, Earth) | as needed |
| AC sockets | Standard wall sockets | 2–4 |
| Logic wire | Jumper/hookup wire 22–24 AWG | — |
| AC wire | 1.5mm² rated for your load | — |

### GPIO Pin Map

| NodeMCU Label | GPIO | Connected To |
|---|---|---|
| **D5** | GPIO 14 | Relay 1 IN |
| **D7** | GPIO 13 | Relay 2 IN |
| **D6** | GPIO 12 | Relay 3 IN |
| **D0** | GPIO 16 | Relay 4 IN |
| **GND** | Ground | All GNDs |
| **VIN** | 5V in | Relay VCC, Power module |

---

## 3. Hardware Variants

The firmware supports **6 hardware variants**, selected at compile time via `config.h`:

| Variant | `#define` | Plugs |
|---|---|---|
| 2-Port, No Display | `VARIANT_2_NO_DISPLAY` | 2 |
| 3-Port, No Display | `VARIANT_3_NO_DISPLAY` | 3 |
| 4-Port, No Display | `VARIANT_4_NO_DISPLAY` | 4 |

### Variant-Aware Device ID

Device IDs automatically include a plug-count prefix:
- **2-port:** `SH2-XXXXXXXX`
- **3-port:** `SH3-XXXXXXXX`
- **4-port:** `SH4-XXXXXXXX`

The web dashboard auto-detects the number of outlets from this prefix and adjusts the UI accordingly.

### Headless Operation

The device operates purely via web dashboard control. The WiFi config portal starts automatically on first boot.

---

## 4. Wiring & Assembly

### Power Wiring

```
Option A — Desktop testing:
  USB cable → NodeMCU USB port
  NodeMCU VIN → Relay VCC × 4
  NodeMCU GND → Relay GND × 4

Option B — Inside the plug strip (AC powered):
  Mains Live + Neutral → Hi-Link AC-DC 5V module
                          ├─► 5V out → NodeMCU VIN
                          ├─► 5V out → Relay VCC × 4
                          └─► GND   → NodeMCU GND + Relay GND × 4
```

> **Note:** Do NOT power relays from 3.3V.

### Relay Module Wiring (Logic Side)

```
Relay VCC  →  5V (VIN)
Relay GND  →  GND
Relay 1 IN →  GPIO14 (D5)
Relay 2 IN →  GPIO13 (D7)
Relay 3 IN →  GPIO12 (D6)
Relay 4 IN →  GPIO16 (D0)
```

### Relay Module Wiring (AC Mains Side)

```
Relay COM  →  Mains LIVE wire (from wall)
Relay NO   →  Socket LIVE terminal
Relay NC   →  Leave disconnected

Neutral wire → directly to all sockets (bypasses relays)
Earth wire   → directly to all socket earth terminals
```

### Push Button Wiring

```
Button UP    →  TX (GPIO1)  →  GND
Button DOWN  →  RX (GPIO3)  →  GND
Button OK    →  D4 (GPIO2)  →  GND
```

Internal pull-up resistors are enabled in firmware. No external resistors needed.

### Boot Pin Safety Notes

| Pin | Boot Requirement | Status |
|---|---|---|
| GPIO0 (D3) | Must be HIGH at power-on | Unused — safe |
| GPIO2 (D4) | Must be HIGH at power-on | Button is only LOW when pressed — safe |
| GPIO15 (D8) | Must be LOW at power-on | Intentionally unused |
| GPIO1 (TX) | Serial transmit | Button UP — serial debug disabled in production |
| GPIO3 (RX) | Serial receive | Button DOWN — do not hold during firmware upload |

### Relay Logic

```
Relay IN pin = LOW  (0V)   → Relay coil energises  → COM–NO connected → Socket ON  ✅
Relay IN pin = HIGH (3.3V) → Relay coil de-energises → COM–NO open    → Socket OFF ✅
```

This is **Active LOW** logic: `#define RELAY_ACTIVE_LOW true`

---

## 5. Firmware Architecture

### Module Structure

```
smart-multiplug-firmware-verient/
├── smart-multiplug-firmware-verient.ino  ← setup/loop orchestration only
├── config.h           ← Pin definitions, constants, variant selection
├── identity.h/cpp     ← LittleFS identity + Firebase registration
├── wifi_setup.h/cpp   ← WiFiManager captive portal + recovery
├── mqtt_client.h/cpp  ← MQTT connect, subscribe, publish, unpair
├── relays.h/cpp       ← setPlug(), state array, flash persistence
├── buttons.h/cpp      ← Debounce + nested menu navigation
├── time_sync.h/cpp    ← NTP sync, timeIsValid tracking
├── scheduling.h/cpp   ← Per-port multi-slot recurring schedules
├── timers.h/cpp       ← Per-port millis()-based countdown timers
├── menu_structure.md  ← Human-readable menu map
└── wiring_guide.md    ← Hardware wiring reference
```

### Required Arduino Libraries

| Library | Version | Purpose |
|---|---|---|
| ESP8266WiFi | (bundled) | WiFi connectivity |
| WiFiManager | v2.0+ (tzapu) | Captive-portal WiFi provisioning |
| PubSubClient | by Nick O'Leary | MQTT client |
| ArduinoJson | v6+ | JSON parsing and generation |
| LittleFS | (bundled) | Persistent config storage |
| Adafruit GFX | by Adafruit | Graphics primitives |
| NTPClient | by Arduino Libraries | NTP time synchronisation |
| Time | by PaulStoffregen | Wall-clock time keeping |

### Arduino IDE Board Settings

| Setting | Value |
|---|---|
| Board | NodeMCU 1.0 (ESH-12E) |
| Flash Size | 4MB (FS:2MB, OTA:~1MB) |
| CPU Frequency | 80 MHz |
| Upload Speed | 115200 baud |
| LittleFS | Enabled |

### Boot Sequence

```
2. loadIdentity()          → Load device ID/secret from LittleFS
                             (self-generates if missing, halts if fatal)
3. initRelays()            → Set GPIO outputs, restore last-known relay states
4. initButtons()           → Configure button pins as INPUT_PULLUP
5. initWiFi()              → Attempt connection or show menu/portal
6. initTimeSync()          → Initialize NTP client (sync on WiFi connect)
6. initScheduling()        → Load persisted schedules from LittleFS
7. initTimers()            → Clear all delay timer slots
8. initMqtt()              → Configure MQTT client (connects in processMqtt)
9. → Transition to SCREEN_STATE_MAIN
```

### Main Loop Processing Order

```
processWiFi()       → WiFi reconnection / portal management
processMqtt()       → MQTT keep-alive + incoming messages
processTimeSync()   → NTP re-sync (hourly when connected)
processSchedules()  → Evaluate daily ON/OFF against wall-clock
processTimers()     → Check countdown expiry, fire setPlug()
processButtons()    → Debounce, long-press, menu navigation
checkForOTAUpdate() → Placeholder for future OTA
yield()             → ESP8266 background tasks (watchdog-safe)
```

### Persistent Storage Files (LittleFS)

| File | Contents |
|---|---|
| `/identity.json` | `device_id`, `device_secret`, `paired_uid`, `registered` |
| `/plug_states.json` | Boolean ON/OFF state per plug (survives reboot) |
| `/schedules.json` | All schedule entries (plug, slot, time, action, repeat) |
| `/wifi_pref.json` | `offline_mode` flag |

---

## 6. WiFi Setup & Recovery

### First Boot (No Saved Credentials)

1. Display model: Device opens the **main menu** so the user can navigate to WiFi Connect → Setup WiFi
2. No-display model: Device automatically starts the WiFi configuration portal AP

### WiFi Configuration Portal

1. Device creates AP named `SmartHome-Setup-<DeviceID>` (e.g., `SmartHome-Setup-SH4-A1B2C3D4`)
2. User connects phone/laptop to this AP (no password)
3. A captive portal page opens automatically (or navigate to `192.168.4.1`)
4. Portal shows device ID, setup instructions, and available WiFi networks
5. User selects their home WiFi, enters password, saves
6. Device connects to home WiFi and shows the **Pairing Code** screen

### WiFi Failure Recovery (Automatic)

- If WiFi connection fails after **3 consecutive attempts** (spaced 10 seconds apart), the device automatically falls back into the WiFi configuration portal AP
- No button press is required for this recovery — it's fully automatic
- Device handles the situation gracefully

### Offline Mode

- The device supports running indefinitely without WiFi ("offline mode")
- If the user uses "Forget WiFi" from the menu, `offlineMode` is set to `true` and persisted
- The device will not nag about WiFi setup on subsequent boots
- All button/relay/timer operations continue working offline
- Delay timers (millis-based) work without WiFi; only Schedule timers require WiFi

### Key Behaviours During WiFi Loss

- **Relays retain their last known ON/OFF state** — WiFi loss never forces plugs off
- **Button control remains fully operational** throughout any WiFi failure
- **MQTT publishes queue only the latest state** per plug (no unbounded memory growth)

---

## 6. MQTT Communication Protocol

### Broker Configuration

| Parameter | Value |
|---|---|
| Host | HiveMQ Cloud |
| Port (Firmware) | 8883 (TLS) |
| Port (Web App) | 8884 (WebSocket Secure, `wss://`) |
| Authentication | Device ID as client ID, configurable username/password |
| Reconnection | Exponential backoff, capped at 60 seconds |
| TLS Buffer | 2048 bytes RX, 1024 bytes TX (to fit ESP8266 heap) |

### Topic Structure

All topics are scoped under `users/{ownerUid}/devices/{deviceId}/`:

#### Subscribe Topics (Device listens)

```
users/+/devices/{deviceId}/plug/+/set            ← ON/OFF command
users/+/devices/{deviceId}/plug/+/timer/set       ← Start/cancel delay timer
users/+/devices/{deviceId}/plug/+/schedule        ← Schedule config (JSON array)
users/+/devices/{deviceId}/sync                   ← Pairing handshake / ping
users/+/devices/{deviceId}/unpair                 ← Unpair command
```

The `+` wildcard allows commands to arrive before the device knows the owner's UID.

#### Publish Topics (Device sends, retained)

```
users/{uid}/devices/{deviceId}/plug/{n}/state       ← "1" or "0"
users/{uid}/devices/{deviceId}/plug/{n}/timer/state  ← JSON countdown state
users/{uid}/devices/{deviceId}/plug/{n}/schedule     ← JSON schedule array
users/{uid}/devices/{deviceId}/status                ← "online" / LWT "offline"
devices/{deviceId}/unpair_event                      ← Unpair notification
```

### Last Will and Testament (LWT)

On connection, the MQTT client sets:
- **LWT topic:** `users/{uid}/devices/{deviceId}/status`
- **LWT message:** `"offline"` (retained)
- **On connect publish:** `"online"` (retained)

This ensures the web dashboard immediately knows when the device goes offline unexpectedly.

### Payload Formats

**Plug State:** `"1"` (ON) or `"0"` (OFF)

**Timer State (JSON):**
```json
{
  "active": true,
  "remainingSeconds": 245,
  "action": "OFF"
}
```

**Schedule (JSON array):**
```json
[
  { "time": "08:00", "action": "ON", "repeat": "daily", "enabled": true },
  { "time": "22:30", "action": "OFF", "repeat": "once", "enabled": true }
]
```

**Timer Set Command (JSON):**
```json
{ "action": "ON", "durationSeconds": 300, "timestamp": 1692000000000 }
```

**Timer Cancel Command (JSON):**
```json
{ "cancel": true }
```

### Sync Mechanism

The web dashboard sends a `sync` message when it first loads. The device responds by re-publishing all current states (plugs, timers, schedules, online status), and the web app uses the response timing to determine if the device is truly online.

### Pairing Handshake

When the device first receives a message on the wildcard subscription `users/+/devices/{deviceId}/...`, it extracts the owner UID from the topic path, saves it to LittleFS as `paired_uid`, and re-subscribes to the owner-specific topic tree. This eliminates the need for manual UID entry on the device.

---

## 7. Timer System (Delay Mode)

### How It Works

- User selects an **action** (Turn ON or Turn OFF) and a **duration**
- Implemented with `millis()`-based countdown — **works without WiFi/NTP**
- Quick-pick presets: 1 / 5 / 15 / 30 min, 1 hour
- Custom duration: 1–999 minutes (adjustable with Up/Down buttons on device, or typed in on web)
- Only **one active timer per plug** — starting a new one replaces the existing one

### Timer Lifecycle

```
User sets timer → startDelayTimer() → millis countdown begins
                                     → MQTT publishes timer/state (retained)
                                     

Timer running   → processTimers()    → publishes remaining time every loop
                                     → web UI shows live countdown

Timer fires     → setPlug()          → relay toggled
                                     → MQTT state published
                                     → timer cleared
                                     → "T" indicator removed
```

### Important Notes

- Timer state does **NOT survive a reboot** — this is intentional and documented
- On reboot, all active delay timers are silently cancelled
- Delay timers publish remaining-time updates via MQTT for live web UI countdown
- Timers from the web dashboard are sent via MQTT `plug/{n}/timer/set`
- millis() overflow is handled correctly (works for ~49 days continuously)

---

## 8. Schedule System

### How It Works

- User selects an **action** (ON/OFF), **time** (HH:MM), and **repeat mode** (Daily/Once)
- Each plug supports up to **4 independent schedule slots** (`MAX_SCHEDULES_PER_PLUG = 4`)
- Requires WiFi + successful NTP sync to configure and fire
- Persisted to LittleFS (`/schedules.json`) and survives reboots
- Schedule entries sync bidirectionally between device and web app via MQTT

### Schedule Entry Structure

| Field | Type | Description |
|---|---|---|
| `enabled` | bool | Whether this slot is active |
| `hour` | uint8 | 0–23 |
| `minute` | uint8 | 0–59 |
| `turnOn` | bool | true = turn ON, false = turn OFF |
| `repeatDaily` | bool | true = fires every day, false = fires once then disables |
| `firedToday` | bool | Internal flag to prevent double-firing within the same minute |

### Schedule Evaluation

```
processSchedules() runs every loop iteration:
  1. Skip if time is not valid (no NTP sync)
  2. Reset all firedToday flags at midnight (new day detected)
  3. Check current hour:minute against all enabled slots (once per minute)
  4. If match found → setPlug() → mark firedToday
  5. If "once" schedule → clear entire slot after firing → persist to flash
  6. Publish updated schedule state via MQTT
```

### Web Dashboard Schedule Features

The web dashboard supports additional scheduling capabilities:
- **Custom day-of-week selection** (Mon, Tue, Wed, Thu, Fri, Sat, Sun)
- **Multiple schedule entries per plug** (stored in Firebase RTDB)
- **Live clock display** with "Set to Now" button
- **Timer preview** showing what will happen before confirming
- **Auto-cleanup** of completed "once" schedules from Firebase (after a 15-second grace period)

### Schedule Actions Menu (Device)

From the physical menu, users can:
- View active slots count (e.g., "Active: 2/4 slots")
- **Start All Slots** — enable all configured slots
- **Stop All Slots** — disable all slots for a port
- **Toggle individual slots** — enable/disable each slot independently
- View slot details including time, action (ON/OFF), and mode (Daily/Once)

---

## 9. Web Dashboard (React App)

### Technology Stack

| Technology | Purpose |
|---|---|
| React 18 | UI framework |
| Vite | Build tool and dev server |
| Firebase Auth | Email/password + Google sign-in |
| Firebase Realtime Database | User data, device configs, schedules |
| Firebase Cloud Functions | Secure device claim/unclaim |
| MQTT.js | MQTT over WebSocket (wss://) |
| React Hot Toast | Notification toasts |
| React Router | Client-side routing |

### Pages

| Page | Route | Description |
|---|---|---|
| Login | `/login` | Email/password + Google sign-in + password reset |
| Sign Up | `/signup` | Account registration with password strength validation |
| Pair Device | `/pair` | Device ID entry + multi-step pairing flow |
| Dashboard | `/` | Real-time plug control with toggle cards |
| Timers | `/timers` | Delay + Schedule timer configuration panel |
| Settings | `/settings` | Account, device, sharing, appearance settings |

### Dashboard Features

- **Plug Cards** with optimistic toggle (instant UI update, reverts if device doesn't respond within 5 seconds)
- **Active/Total outlet counters** in stats bar
- **"Turn All On/Off"** batch toggle button
- **Tap-to-rename** outlets (saved to Firebase, synced across family members)
- **Live timer/schedule indicators** on each plug card
- **Device offline warning banner** with troubleshooting guidance
- **MQTT connecting banner** while establishing broker connection
- **Multi-device dropdown** selector (when multiple plugs are paired)
- **Active sync ping** on page load + periodic heartbeat every 15 seconds

### Timers Page Features

- **Mode Toggle**: Switch between Delay and Schedule modes
- **Outlet Selector**: Choose which plug to configure (uses custom plug labels)
- **Action Toggle**: Turn ON or Turn OFF
- **Delay Presets**: 1, 5, 15, 30 min, 1 hour quick-picks + custom input
- **Schedule Time Picker**: Native time input with live system clock display
- **"Set to Now" Button**: Instantly fills the time picker with current time
- **Repeat Options**: Once / Every Day / Custom Days (Mon–Sun circle buttons)
- **Timer Preview Card**: Shows exactly what will happen before confirming
- **Active Timers List**: Live countdown display for all running timers and schedules
- **Device/MQTT schedule merge**: Shows timers from both Firebase RTDB and device MQTT topics
- **Offline-aware**: Warns when device is offline but still queues commands (MQTT retained)
- **Replacement confirmation**: Prompts before replacing an existing active delay timer

### Settings Page Features

- **Device Info**: Device ID, online/offline status indicator
- **Device Name**: Rename the Smart Home (e.g., "Living Room Plug")
- **Rename Outlets**: Individual inline rename for each socket (e.g., "Desk Lamp", "Fan")
- **Change Password**: With real-time password strength indicators (8+ chars, uppercase, number, symbol)
- **Theme Toggle**: Light / Dark / System mode
- **Language Toggle**: English / বাংলা (Bangla) — persisted to localStorage
- **PWA Install Card**: Guided installation for iOS (Safari share menu), Android (browser install), Desktop
- **Help & Instructions**: 7-step expandable setup guide with numbered icons
- **User Manual Download**: Link to Bangla PDF manual
- **Unpair Device**: Confirmation modal + MQTT unpair notification to device
- **Sign Out**: With confirmation modal
- **Pairing Requests Section** (owner only):
  - View pending requests with guest name/email
  - Accept or reject each request
- **Shared Members Section** (owner only):
  - View all shared member names/emails
  - Revoke access button per member

### Bilingual Support

The entire web UI supports two languages with complete translations:
- **English (en)**: Default for new users
- **Bangla (bn)**: Default language (configurable via `localStorage`)

All labels, messages, toasts, error messages, and help text are translated.

### PWA (Progressive Web App)

The dashboard can be installed as a native-like app:
- **Android**: Browser menu → "Add to Home Screen" or Install App
- **iOS**: Safari → Share → "Add to Home Screen"
- **Desktop**: Browser URL bar → Install icon

Provides fullscreen experience, faster load times, and home screen icon.

### Design System

- **Dark mode** default with glassmorphism effects
- **Micro-animations**: slide-up, fade-in, pulse effects
- **Mobile-first** responsive design
- **CSS custom properties** for consistent theming (`--clr-accent`, `--clr-surface`, etc.)
- **Premium aesthetic**: Gradient accents, smooth transitions, card-based layout
- **Google Fonts**: Inter / system font stack

---

## 10. Device Pairing & Family Sharing

### Initial Pairing Flow

```
1. User creates account or signs in on web dashboard
2. User navigates to Pair Device page
3. User enters Device ID (shown on device sticker or AP name)
   - Accepts formats: SHX-XXXXXXXX, SH2-..., SH3-..., SH4-...
   - Auto-resolves variant prefix if user enters old SH- format
   - Also accepts raw 8-char hex (auto-prepends "SH-")
4. System checks if device exists in Firebase /devices table
5. If unclaimed → claims device immediately (atomically via Cloud Function)
6. If already claimed → creates pairing request for owner approval
6. On success → web app sends MQTT sync message to device
7. Device extracts owner UID from sync topic and saves it
8. Both sides are now linked and can communicate
```

### Family Sharing Architecture

#### Pairing Request Flow

```
Guest enters Device ID → Device already claimed
  ↓
Guest's request written to: devices/{deviceId}/pairing_requests/{guestUid}
  with: { email, name, requested_at, status: "pending" }
  ↓
Guest sees "Awaiting Approval" spinner (real-time Firebase listener)
  ↓
Owner sees pending request in Settings → Accept or Reject
  ↓
Accept: 
  - Guest added to devices/{deviceId}/shared_members/{guestUid}
  - Device link created at users/{guestUid}/devices/{deviceId}
    with: { is_shared: true, owner_uid: "...", paired_at: ... }
  - Pairing request node deleted
  - Guest auto-redirects to paired success screen (real-time listener)

Reject:
  - Pairing request node deleted
  - Guest sees "Request Rejected" screen with retry option
```

#### Shared Member Permissions

- Shared members can control plugs, set timers, and view schedules
- All MQTT commands publish under the **owner's UID** topic (not the guest's)
- The `DeviceContext` resolves `ownerUid` dynamically for shared devices
- The owner can revoke any shared member's access at any time from Settings

### Unpair Flow

Unpairing can be triggered from:
1. **Web dashboard** (Settings → Unpair Device) — sends MQTT `unpair` retained message
2. **Physical device** (Menu → App Connect → Unpair Device) — publishes `unpair_event`

Both paths:
- Clear all retained MQTT messages (status, plug states, timer states, schedules)
- Reset Firebase database claim (`claimed_by: null`)
- Remove user device reference from `users/{uid}/devices/`
- Remove shared members (owner unpair only)
- Clear local `pairedUid` on device and re-enter wildcard subscription mode

The device publishes an `unpair_event` on both `users/{uid}/devices/{deviceId}/unpair_event` and `devices/{deviceId}/unpair_event` topics so the web app can clean up regardless of which user is online.

---

## 11. Firebase Backend

### Realtime Database Structure

```json
{
  "devices": {
    "SH4-A1B2C3D4": {
      "device_secret": "sec-123456789012",
      "claimed_by": "uid_of_owner",
      "claimed_at": 1700000000000,
      "shared_members": {
        "uid_of_guest": {
          "email": "guest@email.com",
          "name": "Guest Name",
          "shared_at": 1700000000000
        }
      },
      "pairing_requests": {
        "uid_of_requester": {
          "email": "requester@email.com",
          "name": "Requester Name",
          "requested_at": 1700000000000,
          "status": "pending"
        }
      }
    }
  },
  "users": {
    "uid_of_owner": {
      "devices": {
        "SH4-A1B2C3D4": {
          "name": "Living Room Plug",
          "paired_at": 1700000000000,
          "plugs": {
            "1": { "label": "Desk Lamp" },
            "2": { "label": "Fan" },
            "3": { "label": "Charger" },
            "4": { "label": "Monitor" }
          },
          "schedules": {
            "-NxxxxxxxxxA": {
              "plugNum": 1,
              "plugLabel": "Desk Lamp",
              "action": "OFF",
              "time": "23:00",
              "repeat": "daily",
              "days": "all",
              "enabled": true,
              "createdAt": 1700000000000,
              "type": "schedule"
            }
          }
        }
      }
    }
  }
}
```

### Cloud Functions

| Function | Type | Purpose |
|---|---|---|
| `claimDevice` | HTTPS Callable | Securely pairs a device to the authenticated user's account via atomic multi-path write |
| `unclaimDevice` | HTTPS Callable | Removes a device from the authenticated user's account with ownership verification |

Both functions automatically receive the authenticated user context — UIDs are never trusted from the client. If Cloud Functions are unavailable (e.g., during development), the web app falls back to direct database writes.

### Database Security Rules

- Users can only read/write their own `users/{uid}` path
- Device owners (`claimed_by === auth.uid`) can manage shared members and pairing requests
- Guests can submit pairing requests but cannot modify the device claim
- Shared members can read the owner's device config path
- All authenticated users can read device existence (for pairing lookup)

### Device Self-Registration in Firebase

On first WiFi connection, the device automatically registers itself in Firebase Realtime Database via a direct HTTPS PUT:

```
PUT https://{db-url}/devices/{deviceId}.json?auth={device_secret}
{ "device_secret": "...", "claimed_by": null }
```

This ensures the device exists in the database before any user tries to pair it. If the database is wiped, the device re-creates its entry on the next boot with WiFi (`registerDeviceInFirebase()` is called on every successful WiFi connection).

---

## 12. Factory Provisioning & Flashing

### Flash Tool (`flash.sh`)

A shell script for compiling and uploading firmware:

```bash
./flash.sh            # Auto-detect port, compile & upload
./flash.sh -p /dev/cu.usbserial-XXX  # Specify port manually
./flash.sh -c         # Compile only, no upload
./flash.sh -m         # Open Serial Monitor after upload
./flash.sh -h         # Show help
```

**Requirements:** `arduino-cli` (install via `brew install arduino-cli`)

### Identity Generation

On first boot (no `/identity.json`), the firmware auto-generates:
- **Device ID**: `SP{N}-{8-hex-chars}` derived from the ESP8266 chip ID and plug count
- **Device Secret**: Random string using `micros()` + `analogRead(A0)` as entropy seed

These are saved to LittleFS and persist across reboots. Only a factory reset (Up + OK 15 seconds) clears them.

### Variant Selection

Before compiling, set the active variant in `config.h`:
```cpp
#define ACTIVE_VARIANT VARIANT_4_WITH_DISPLAY
```

And the screen driver:
```cpp
#define ACTIVE_SCREEN_DRIVER SCREEN_DRIVER_SH1106  // or SCREEN_DRIVER_SSD1306
```

### Pre-Flight Test Procedure

1. Flash firmware to NodeMCU via `./flash.sh` or Arduino IDE
2. Power on — verify WiFi config AP appears
3. Press OK → enters menu
4. Navigate to each Port → Toggle Power → hear relay click
5. **Test all relays before connecting any mains voltage**
6. Navigate to WiFi Connect → Setup WiFi → configure home WiFi
6. Verify WiFi connects (W indicator on dashboard)
7. Navigate to App Connect → verify Device ID is displayed
8. Pair device on web dashboard using the Device ID
9. Toggle outlets from web dashboard → verify relays respond

---

## 13. Security Architecture

### MQTT Security

| Layer | Implementation |
|---|---|
| Transport | TLS 1.2 (port 8883 for firmware, wss://8884 for web) |
| Authentication | Username/password per broker credentials |
| Certificate | `setInsecure()` on ESP8266 to save RAM (known tradeoff) |
| Topic Scoping | All topics under `users/{uid}/devices/{deviceId}/` |
| Buffer Limits | TLS buffers limited to prevent ESP8266 OOM |

**Current tradeoff (MVP):** Static MQTT credentials in web app environment variables are visible in client-side JS. Acceptable because credentials are scoped to a single read/write user, not broker admin.

**Production hardening:** Issue short-lived MQTT credentials from a Cloud Function on login using HiveMQ's REST API or a custom JWT auth plugin.

### Firebase Security

- Firebase Auth handles all user authentication (email/password + Google OAuth)
- Cloud Functions verify `request.auth.uid` server-side before any database write
- Database rules enforce ownership checks (`claimed_by === auth.uid`)
- Device secrets are stored in Firebase but are only used for device self-registration
- Password strength enforcement: 8+ chars, uppercase, number, symbol (enforced client-side)

### Device Identity

- `device_id` and `device_secret` are generated per-device and stored on LittleFS
- The secret is used for Firebase registration but is NOT the MQTT password
- Factory reset formats LittleFS and clears WiFi credentials → new identity generated on next boot

---

## 14. Troubleshooting Guide

### Device Won't Connect to WiFi

1. Check that the WiFi password is correct
2. Ensure the router supports **2.4 GHz** (ESP8266 does not support 5 GHz)
3. Reset WiFi preferences and setup WiFi again
4. After 3 failed attempts, the device automatically opens the setup portal
5. Check that router MAC filtering is not blocking the ESP8266

### Relays Not Clicking

1. Verify relay module is powered with **5V** (VIN pin), not 3.3V
2. Check wiring between NodeMCU GPIO pins and relay IN pins
3. Test from the Web Dashboard: Port X → Toggle Power
4. If relay clicks but socket doesn't work, check AC wiring (COM → Live, NO → Socket)

### Schedule Not Firing

1. Schedule mode requires WiFi and successful NTP time sync
2. Check that "WiFi: Connected" is shown in the Schedule Timer menu
3. Verify the time zone offset is correct (`NTP_OFFSET_SEC` in config.h, default: +6h UTC = Bangladesh)
4. If the device lost power, schedules require a fresh NTP sync before they fire
5. Use **Delay mode** for offline timer functionality (doesn't need NTP)
6. Check the "Active: X/4 slots" counter in the Schedule Actions menu

### Timer Not Starting from Web App

1. Ensure the device shows as "Online" on the dashboard
2. Check that MQTT is connected (the "Connecting to your device…" banner should not be visible)
3. Timers sent while the device is offline will be received when it reconnects (MQTT retained)

### Pairing Issues

1. Ensure the Device ID format is correct: `SHX-XXXXXXXX` (X = number of plugs, 2/3/4)
2. The device must be registered in Firebase (happens automatically on first WiFi connection)
3. If the device was previously paired to another account, the new user will see "Awaiting Approval"
4. The owner must approve the pairing request from their Settings page
5. If rejected, the guest can try again with the correct Device ID

### Web App Shows "Configuration Required"

1. Copy `.env.example` to `.env` in the web project directory
2. Fill in all Firebase and MQTT credentials
3. Restart the development server (`npm run dev`)

---

## 14. Known Limitations

| Limitation | Impact | Workaround |
|---|---|---|
| No RTC chip | Schedule timers require WiFi for NTP time sync | Use Delay mode for offline timing |
| No 5 GHz WiFi | ESP8266 only supports 2.4 GHz networks | Ensure router has 2.4 GHz band enabled |
| Delay timers don't survive reboot | Active countdowns are lost on power cycle | Use Schedule mode for persistent automation |
| TLS certificate not validated | `setInsecure()` used to save RAM | Acceptable for HiveMQ Cloud; use fingerprint if self-hosting |
| Static MQTT credentials in web app | Visible in browser source code | Use Cloud Function-issued tokens in production |
| No OTA updates | Firmware must be flashed via USB cable | Placeholder `checkForOTAUpdate()` exists for future implementation |
| 4 schedule slots per plug (device) | Web dashboard supports unlimited via Firebase | Device syncs up to 4 from MQTT; additional web-only schedules fire through MQTT commands |
| Serial debug disabled in production | TX/RX pins used for buttons | Enable `DEBUG_SERIAL` in config.h and use alternative button pins for debugging |
| No energy monitoring | Cannot measure power consumption | Planned for future hardware revision |
| No push notifications | Timer/schedule events only visible if app is open | Planned for future addition |

---

## Appendix A: Configuration Constants (config.h)

| Constant | Default Value | Description |
|---|---|---|
| `FIRMWARE_VERSION` | `"v1.6.0"` | Displayed on About screen |
| `RELAY_ACTIVE_LOW` | `true` | Relay trigger polarity |
| `SCREEN_INACTIVITY_TIMEOUT_MS` | `10000` | Menu auto-dismiss (10s) |
| `DEBOUNCE_TIME_MS` | `40` | Button debounce (40ms) |
| `LONG_PRESS_BACK_MS` | `1000` | OK long-press for back (1s) |
| `FACTORY_RESET_HOLD_MS` | `15000` | Up+OK factory reset (15s) |
| `WIFI_MAX_RETRIES` | `3` | WiFi reconnect attempts before portal fallback |
| `WIFI_RETRY_INTERVAL_MS` | `10000` | WiFi retry spacing (10s) |
| `MQTT_PORT` | `8883` | TLS port |
| `MQTT_RECONNECT_INTERVAL_MS` | `5000` | MQTT initial retry (5s) |
| `NTP_SERVER` | `"pool.ntp.org"` | Time server |
| `NTP_OFFSET_SEC` | `21600` | UTC+6 (Bangladesh Standard Time) |
| `NTP_UPDATE_INTERVAL_MS` | `3600000` | NTP re-sync interval (1 hour) |
| `MAX_SCHEDULES_PER_PLUG` | `4` | Schedule slots per plug |
| `FILE_IDENTITY` | `"/identity.json"` | LittleFS identity file |
| `FILE_PLUG_STATES` | `"/plug_states.json"` | LittleFS relay state file |
| `FILE_SCHEDULES` | `"/schedules.json"` | LittleFS schedules file |
| `FILE_WIFI_PREF` | `"/wifi_pref.json"` | LittleFS WiFi preferences file |

## Appendix B: Web App Environment Variables

| Variable | Where to Find |
|---|---|
| `VITE_FIREBASE_API_KEY` | Firebase Console → Project Settings → Your apps |
| `VITE_FIREBASE_AUTH_DOMAIN` | Same |
| `VITE_FIREBASE_DATABASE_URL` | Firebase Console → Realtime Database → Data tab (URL at top) |
| `VITE_FIREBASE_PROJECT_ID` | Firebase Console → Project Settings |
| `VITE_FIREBASE_STORAGE_BUCKET` | Same |
| `VITE_FIREBASE_MESSAGING_SENDER_ID` | Same |
| `VITE_FIREBASE_APP_ID` | Same |
| `VITE_MQTT_BROKER_URL` | HiveMQ Cloud cluster URL: `wss://xxxx.hivemq.cloud:8884/mqtt` |
| `VITE_MQTT_USERNAME` | HiveMQ Cloud → Access Management → Credentials |
| `VITE_MQTT_PASSWORD` | Same |

## Appendix C: Web App Deployment

```bash
# Install dependencies
npm install

# Run locally
npm run dev

# Build for production
npm run build

# Deploy to Firebase Hosting
firebase deploy --only hosting

# Deploy Cloud Functions
cd functions && npm install
cd .. && firebase deploy --only functions

# Deploy Database Rules
firebase deploy --only database
```

---

*This manual covers firmware v1.6.0 and the corresponding web dashboard. For the Bangla user guide, see `Smart_MultiPlug_User_Manual_Bangla.pdf`.*
