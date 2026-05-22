# The Notorious E.S.P.

A bare-metal Wi-Fi packet sniffer for the ESP8266, built entirely from scratch. Inspects raw wireless frames, parses network traffic, and filters packets using a custom rule engine — all without a single dynamic memory allocation.

No malloc. No RTOS. No mercy.

---

## How It Works

The ESP8266's radio is placed in **promiscuous mode**, allowing it to capture raw 802.11 frames off the air regardless of destination. From there, four components work in a strict pipeline:

### 1. Static Slab Allocator (`memory_pool`)
A fixed-size pool of 16 frame slots, each 256 bytes. All memory is reserved at link time in the BSS segment. `acquire()` returns a pointer to a free slot in O(n) time; `release()` marks it free again. Zero heap involvement at any point.

### 2. Lock-Free Ring Buffer (`ring_buffer`)
Transfers slot indices between the ISR callback and the main processing loop using a power-of-two indexed circular buffer with `volatile` head and tail. Bitmask wraparound instead of modulo — every cycle counts in interrupt context.

### 3. 802.11 Frame Parser (`frame_parser`)
A stateless, zero-copy parser that manually walks raw frame bytes through:
- 12-byte `RxControl` metadata block (the prefix NONOS_SDK prepends to every promiscuous-mode buffer — RSSI, rate, channel, etc.)
- 802.11 MAC header (24 or 30 bytes depending on DS flags)
- LLC/SNAP header (8 bytes)
- IPv4 header (IHL-aware, variable length)
- TCP or UDP transport header

Extracts source/destination IPs, ports, protocol, and a pointer directly into the original frame buffer. No copying. Every buffer access is bounds-checked against the per-slot frame length tracked by `MemoryPool`.

### 4. Bitmask Rule Engine (`filter_engine`)
A static table of up to 16 rules, each matched using bitmask arithmetic against source IP, destination port, and protocol. First-match semantics. Actions: `LOG`, `COUNT`, or `IGNORE`. Unmatched frames are logged with a warning and allowed through by default.

---

## Project Structure

```
packet_inspector/
├── Makefile
├── include/
│   ├── memory_pool.hpp
│   ├── ring_buffer.hpp
│   ├── frame_parser.hpp
│   ├── filter_engine.hpp
│   └── user_config.h
└── src/
    ├── user_main.cpp
    ├── memory_pool.cpp
    ├── ring_buffer.cpp
    ├── frame_parser.cpp
    └── filter_engine.cpp
```

`user_main.cpp` owns the boot sequence (partition-table registration, GPIO setup, deferred WiFi init, channel-hopping timer) and the promiscuous-mode callback that feeds the pipeline. The four core components above sit behind it as the data path.

---

## Building

Requires the Espressif Non-OS SDK v3.0.5 and the Xtensa GCC 8.4.0 toolchain.

```bash
# Build
make

# Flash app + RF cal partition (rf_cal / phy_data / sys_param sectors)
make flash

# Monitor serial output (74880 baud — ESP8266 ROM rate with a 26 MHz crystal)
make monitor
```

`make flash` writes `esp_init_data_default_v08.bin` and `blank.bin` to the high-end of flash alongside the app, so the partition table registered in `user_pre_init` actually points at valid RF-calibration data on a clean chip.

---

## Runtime behavior

Once flashed, the firmware:

1. Registers a partition table (RF_CAL / PHY_DATA / SYSTEM_PARAMETER) and brings up the SDK in `STATION_MODE`.
2. Waits for `system_init_done_cb`, then enables promiscuous mode (configuring it before STA-init completes lets the SDK overwrite the channel and callback).
3. Hops across 802.11 channels 1–13 every 400 ms via an `ets_timer`.
4. Toggles the onboard blue LED (GPIO2, active-LOW) on every received frame, so the LED tracks RF activity visibly.
5. Emits two kinds of log lines on UART0:

```
rx n=64 len=128 type=0 sub=8 prot=0           # every 64th frame: 802.11 metadata
rx proto=6 192.168.1.10:54321 -> 1.1.1.1:443 len=412   # every parsed IPv4/TCP/UDP
```

The 802.11 metadata log shows whether what's around is mostly beacons (`type=0 sub=8`), data (`type=2`), and whether the Protected bit is set. The parsed log only fires when the radio lands on a channel with **unencrypted** IPv4 data — beacons and WPA/WPA2 ciphertext both fall out at the parser stage.

---

## Design Constraints

| Constraint | Detail |
|---|---|
| RAM budget | ~50KB usable after SDK overhead |
| Pool size | 16 × 256 bytes = 4KB |
| Dynamic allocation | None. Ever. |
| C++ exceptions | Disabled (`-fno-exceptions`) |
| RTTI | Disabled (`-fno-rtti`) |
| Optimization | `-Os` (size) |

---

## Target Hardware

Espressif ESP8266 — Tensilica Xtensa LX106 core, 80 MHz, ~80 KB RAM, 4 MB flash. Built and tested on a NodeMCU v1.0 (ESP-12E, 26 MHz crystal). Any ESP-12 / ESP-07 module with comparable flash should work; the partition table assumes a 512+512 layout and a 4 MB flash chip.