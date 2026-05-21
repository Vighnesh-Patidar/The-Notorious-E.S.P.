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
- 802.11 MAC header (24 or 30 bytes depending on DS flags)
- LLC/SNAP header (8 bytes)
- IPv4 header (IHL-aware, variable length)
- TCP or UDP transport header

Extracts source/destination IPs, ports, protocol, and a pointer directly into the original frame buffer. No copying. Every buffer access is bounds-checked against the frame length.

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
│   └── filter_engine.hpp
└── src/
    ├── user_main.cpp
    ├── memory_pool.cpp
    ├── ring_buffer.cpp
    ├── frame_parser.cpp
    └── filter_engine.cpp
```

---

## Building

Requires the Espressif Non-OS SDK v3.0.5 and the Xtensa GCC 8.4.0 toolchain.

```bash
# Build
make

# Flash to device
make flash

# Monitor serial output
make monitor
```

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

## Upcoming

- [ ] Hardware validation and serial output testing
- [ ] Management frame parser for passive BSSID/SSID mapping
- [ ] `constexpr` compile-time rule tables
- [ ] UART command interface for runtime rule updates
- [ ] Deauth frame detection

---

## Target Hardware

Espressif ESP8266 — Tensilica Xtensa LX106 core, 80MHz, ~80KB RAM.