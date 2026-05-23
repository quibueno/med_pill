#  Smart Medication Dispenser (Safety-Hardened Firmware)

Open-loop carousel medication dispenser firmware for the **ESP32**, driving a
**28BYJ-48 stepper** through a **ULN2003** driver. The device releases one dose
at scheduled times, refuses to dispense unless a cup is physically present, and
fails into a safe, non-dispensing state whenever anything looks wrong.

>  **SAFETY NOTICE — READ BEFORE USE**
> This is hobbyist/maker firmware, **not a certified medical device**. It can
> mitigate *software* risks (timing, duplicate doses, uncertain state, hangs)
> but it **cannot** eliminate *mechanical* risk in an open-loop design. A wrong
> dose can harm or kill a vulnerable patient. Have every clinical decision (dose
> timing, late-dose policy, recovery behavior) reviewed by a healthcare
> professional, and supervise real-world use.

---

##  Platform: ESP32 only (not the plain Wemos D1 / D1 mini)

This firmware targets the **ESP32**. It will **not compile or run** on an
ESP8266-based *Wemos D1 / D1 mini*, because it relies on ESP32-only features:

- `GPIO 32` is used for the cup sensor — that pin **does not exist** on the
  ESP8266 (which only exposes GPIO 0–16).
- `WebServer.h` and `Preferences.h` (NVS) are ESP32 APIs; the ESP8266 uses
  `ESP8266WebServer.h` and `EEPROM`/`LittleFS`.

If your board is a **Wemos D1 R32** (an ESP32 in the Uno form factor), you are
fine. If it is a genuine **ESP8266 D1 mini**, a full port to different libraries,
storage, and pin numbers is required.

---

## Features (safety mitigations)

| # | Mitigation | What it prevents |
|---|------------|------------------|
| S1 | **Hardware task watchdog** | A firmware hang silently freezing the device and skipping doses — the board now auto-reboots. |
| S2 | **Web server frozen during motor movement** | A web request mutating motor position mid-dispense (race condition). |
| S3 | **Motor-busy guard on web handlers** | "Zero"/"Confirm"/"Factory reset" altering state while the motor turns. |
| S4 | **Password-protected configuration AP (WPA2)** | A nearby stranger reprogramming medication times over an open network. |
| S5 | **No re-dispense after power-loss recovery** | A double dose (overdose) right after acknowledging a SAFE_MODE event. |
| S6 | **Missed-dose detection + escalating alarm** | Silent failures when the cup is never placed. |
| S7 | **In-RAM recent-event log on the web page** | Caregivers being unable to audit whether doses were released/taken/missed. |
| S8 | **HTML escaping of user input** | Page breakage / injection from crafted SSID, password, or schedule fields. |
| S9 | **Length limits + validation before save** | Memory exhaustion and invalid schedules being persisted. |
| S10 | **Heap-fragmentation reduction (`String.reserve`)** | Crashes after weeks of uptime from fragmented memory. |

Additional always-on protections inherited from the design:

- **Cup interlock:** never dispenses unless a cup is stably detected first.
- **Clock fail-safe:** never dispenses unless time is NTP-reliable.
- **Anti-duplicate persistence:** the last dispensed epoch-minute is stored in
  NVS, so a reboot cannot trigger a second dose within the same minute.
- **Crash detection:** a reboot during an active dispense forces SAFE_MODE,
  which requires human acknowledgment before resuming.
- **Motor timeout:** a stalled/jammed motor trips SAFE_MODE instead of grinding.
- **Long-press physical reset** to wipe configuration and return to setup mode.

---

## Hardware

### Bill of materials

- 1 × **ESP32** board (e.g., Wemos D1 R32, or any ESP32 DevKit)
- 1 × **28BYJ-48** stepper motor (5 V)
- 1 × **ULN2003** driver board
- 1 × **Reed switch** (or magnetic/Hall sensor) for cup detection + magnet
- 1 × Active **buzzer**
- 1 × Momentary **push button** (physical reset)
- 5 V power supply rated for the stepper, and the dispensing carousel mechanism

### Pinout

| Function | GPIO | Mode | Notes |
|----------|------|------|-------|
| Stepper IN1 | 19 | OUTPUT | ULN2003 IN1 |
| Stepper IN2 | 18 | OUTPUT | ULN2003 IN2 |
| Stepper IN3 | 5 | OUTPUT | ULN2003 IN3 |
| Stepper IN4 | 17 | OUTPUT | ULN2003 IN4 |
| Buzzer | 16 | OUTPUT | Active buzzer |
| Cup sensor | 32 | INPUT_PULLUP | `LOW` = cup present (magnet closes the reed switch) |
| Reset button | 4 | INPUT_PULLUP | `LOW` = pressed; hold 5 s to factory-reset |

> The `AccelStepper` object is constructed in `HALF4WIRE` mode with the pin order
> `(IN1, IN3, IN2, IN4)`. If the motor vibrates instead of turning, swap the
> middle pair before changing anything else.

---

## Dependencies

- **Arduino-ESP32 core** (board support package). Both core 2.x and 3.x are
  supported; the watchdog setup auto-selects the correct API at compile time.
- **AccelStepper** library (install via the Arduino Library Manager).
- Standard headers `WiFi.h`, `WebServer.h`, `Preferences.h`, `time.h`,
  `math.h`, and `esp_task_wdt.h` ship with the ESP32 core.

---

## Build and flash

1. Install the **Arduino-ESP32** core in the Arduino IDE (Boards Manager) or use
   PlatformIO.
2. Install the **AccelStepper** library.
3. Open `medpill_v22.ino`.
4. **Before flashing, change `AP_PASS`** from its placeholder to a strong value
   (WPA2 requires at least 8 characters).
5. Select your ESP32 board and the correct serial port, then upload.
6. Open the Serial Monitor at **115200 baud** to watch boot logs.

> The firmware was authored without an on-hand ESP32 toolchain, so **compile and
> bench-test it yourself** before any real use. If the watchdog section fails to
> compile on your core version, set `#define ENABLE_TASK_WDT 0` to disable it and
> report the core version so it can be adapted.

---

## First-time setup

1. On first boot (no Wi-Fi saved), the device starts an access point named
   **`Dispensador_Config`** protected by `AP_PASS`.
2. Connect to that network and open **`http://192.168.4.1`** in a browser.
3. Enter the **medication schedule**, **Wi-Fi SSID**, and **Wi-Fi password**,
   then press **Save**. The device reboots and joins your network.
4. Once connected, it syncs time over NTP and is ready to dispense.

### Schedule format

Times are 24-hour `HH:MM`, comma-separated, e.g. `08:00,12:00,20:00`.
Input is normalized automatically: spaces are stripped, `;` and `|` are treated
as commas, duplicates/empties are cleaned, and each token is validated. Up to
**24** times are accepted.

---

## How it works (state machine)

| State | Behavior |
|-------|----------|
| `BOOT` | Initial state during startup. |
| `WIFI_SETUP` | Serves the configuration page; **never dispenses**. |
| `IDLE` | Ready. Checks the schedule once per second (only if the clock is reliable). |
| `WAITING_CUP_TO_DISPENSE` | A scheduled time arrived but no cup is present; beeps slowly, then escalates. |
| `DISPENSING` | Advances exactly one slot, with timeout protection. |
| `WAITING_CUP_REMOVAL` | Dose released; beeps fast until the cup is removed. |
| `OUT_OF_SLOTS` | Carousel empty; waits for refill + manual zero. |
| `SAFE_MODE` | A fault was detected; **no automatic dispensing** until a human confirms. |

A normal dose cycle: `IDLE` → (cup present) → `DISPENSING` → `WAITING_CUP_REMOVAL`
→ `IDLE`.

### Buzzer patterns

- **Slow** — please place the cup.
- **Fast** — dose released, please take it / remove the cup.
- **Alarm (rapid)** — a fault occurred (SAFE_MODE).

---

## Web interface

| Route | Method | Purpose |
|-------|--------|---------|
| `/` | GET | Status dashboard, configuration form, and recent-event log. |
| `/save` | POST | Save schedule + Wi-Fi credentials, then reboot. |
| `/zerar` | GET | Force the logical position back to slot 0 (after refilling). |
| `/ack` | GET | Acknowledge and exit SAFE_MODE. |
| `/resetwifi` | GET | Factory reset (clears all stored configuration). |

The dashboard shows current state, time, slot count, clock reliability, the last
dispensed epoch-minute, the in-progress flag, hold-torque status, and the recent
event log.

---

## Key configuration constants

Edit these at the top of the sketch to match your mechanics and policy:

| Constant | Default | Meaning |
|----------|---------|---------|
| `STEPS_PER_REV` | `4096` | Half-step counts per revolution (28BYJ-48). |
| `TOTAL_SLOTS` | `21` | Compartments on the carousel. |
| `SLOT_MAX` | `20` | Last usable slot index. |
| `MOVE_TIMEOUT_MS` | `20000` | Max time to move one slot before SAFE_MODE. |
| `CUP_DEBOUNCE_MS` | `200` | Cup-sensor debounce window. |
| `RESET_HOLD_MS` | `5000` | Hold time for the physical factory reset. |
| `CUP_WAIT_ESCALATE_MS` | `60000` | When the cup-wait alarm escalates from slow to fast. |
| `CUP_WAIT_HARD_TIMEOUT_MS` | `0` | `0` = disabled (give the dose whenever the cup arrives). `>0` = after this delay, log a **missed dose** and skip it. |
| `WDT_TIMEOUT_MS` | `10000` | Watchdog timeout. |
| `GMT_OFFSET_SEC` | `-10800` | Timezone (UTC−3 / Brazil). |
| `EPOCH_MIN_VALID` | `1704067200` | Earliest epoch considered a reliable clock (2024-01-01). |
| `AP_SSID` / `AP_PASS` | `Dispensador_Config` / *placeholder* | Configuration access-point credentials. **Change the password.** |
| `ENABLE_TASK_WDT` | `1` | Set to `0` if the watchdog code does not compile on your core. |
| `HOLD_TORQUE_ENABLED` | `1` | Keep holding torque after a move to resist slippage. |

---

##  Clinical policy decisions to validate

Two behaviors are deliberately conservative and **must be confirmed for each
medication by a healthcare professional**:

1. **Late dose vs. missed dose.** By default (`CUP_WAIT_HARD_TIMEOUT_MS = 0`) a
   dose is released whenever the cup is finally placed, even if late. Setting a
   timeout makes very-late doses count as *missed* and *skipped* instead.
2. **Recovery after power loss.** When the user acknowledges a SAFE_MODE that was
   triggered by an interrupted dispense, the firmware does **not** re-release the
   dose for that same minute. This biases toward *avoiding an overdose* rather
   than *avoiding a skipped dose*. Confirm that this trade-off is correct for the
   drug in question.

---

## Limitations

- **Open-loop, no homing.** The firmware tracks position by counting steps. If
  the carousel slips mechanically, the logical slot can drift from the physical
  slot, and software cannot detect it. For real clinical use, add a **mechanical
  detent/ratchet** and/or a **home reference sensor**.
- **No tamper-proof enclosure or redundancy.** A single mechanical failure has no
  backup.
- **NTP dependency.** Without a reliable synced clock, the device deliberately
  refuses to dispense. A real-time clock (RTC) module is recommended for
  resilience during internet outages.
- **Event log is volatile.** Recent events live in RAM and are lost on reboot.

---

## Troubleshooting

- **Won't dispense at the scheduled time:** check that the clock is reliable
  (dashboard shows `SIM`/yes), Wi-Fi is connected, and a cup is detected.
- **Motor vibrates but doesn't rotate:** swap the middle stepper pin pair
  (`IN3`/`IN2`) in the `AccelStepper` constructor.
- **Stuck in SAFE_MODE:** open the dashboard, physically verify the carousel,
  then press **Confirm**. Investigate the logged reason first.
- **Watchdog compile error:** set `#define ENABLE_TASK_WDT 0`.
- **Forgot Wi-Fi / locked out:** hold the reset button for 5 seconds to wipe
  config and return to the configuration AP.

---

## Disclaimer

This software is provided **as is**, without warranty of any kind. It is not a
medical device and has not been certified for clinical use. The authors and
contributors are not liable for any harm resulting from its use. Use only with
appropriate medical supervision and at your own risk.
