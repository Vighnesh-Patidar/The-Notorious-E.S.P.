#include "beacon.hpp"
extern "C" {
    #include "user_interface.h"
}

void Beacon::send(const uint8_t* payload, uint8_t len)
{
    uint8_t mac[6];
    wifi_get_macaddr(STATION_IF, mac);

    uint8_t buf[128];
    uint8_t pos = 0;

    // Frame Control - Data Frame (Type 2, Subtype 0)
    // 0x08, 0x00 indicates a standard STA-to-STA data frame
    buf[pos++] = 0x08; buf[pos++] = 0x00;
    
    // Duration (Handled by hardware, set to 0)
    buf[pos++] = 0x00; buf[pos++] = 0x00;
    
    // Destination MAC - Broadcast
    buf[pos++] = 0xFF; buf[pos++] = 0xFF;
    buf[pos++] = 0xFF; buf[pos++] = 0xFF;
    buf[pos++] = 0xFF; buf[pos++] = 0xFF;
    
    // Source MAC (Address 2)
    for(int i = 0; i < 6; i++) buf[pos++] = mac[i];
    
    // BSSID (Address 3) - Using Source MAC
    for(int i = 0; i < 6; i++) buf[pos++] = mac[i];
    
    // Sequence Control
    buf[pos++] = 0x00; buf[pos++] = 0x00;

    // Bounds check to prevent buffer overflow
    uint8_t safe_len = len;
    if (safe_len > (sizeof(buf) - pos)) {
        safe_len = sizeof(buf) - pos;
    }

    // Inject the raw binary payload
    for(int i = 0; i < safe_len; i++) {
        buf[pos++] = payload[i];
    }

    // Fire the frame over the air
    wifi_send_pkt_freedom(buf, pos, false);
}