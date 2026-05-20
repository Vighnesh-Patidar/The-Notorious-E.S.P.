#ifndef FRAME_PARSER_H
#define FRAME_PARSER_H

struct ParsedFrame
{
    uint32_t src_ip, dst_ip;
    uint16_t src_port, dst_port;
    uint8_t protocol;
    uint8_t* payload;
    uint16_t payload_length;
};

class FrameParser
{
    static ParsedFrame* parse(const uint8_t* buf, const uint16_t len);
};

#endif