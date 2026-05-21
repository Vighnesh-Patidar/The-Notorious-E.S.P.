#ifndef FRAME_PARSER_H
#define FRAME_PARSER_H

#include "c_types.h"
#include "user_config.h"

struct ParsedFrame
{
    uint32_t src_ip, dst_ip;
    uint16_t src_port, dst_port;
    uint8_t protocol;
    const uint8_t* payload;
    uint16_t payload_length;
};

class FrameParser
{
public:
    static ParsedFrame* parse(const uint8_t* buf, const uint16_t len);
};

#endif
