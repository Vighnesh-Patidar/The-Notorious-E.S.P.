#ifndef BEACON_HPP
#define BEACON_HPP

#include "c_types.h"

// Undocumented Espressif function for raw frame injection
extern "C" int wifi_send_pkt_freedom(uint8_t* buf, int len, bool sys_seq);

class Beacon
{
public:
    // Takes a raw binary array and its length for packet injection
    static void send(const uint8_t* payload, uint8_t len);
};

#endif