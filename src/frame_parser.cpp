#include "frame_parser.hpp"
#include "c_types.h"

// NONOS_SDK promisc buffer prefix: 12-byte RxControl metadata, then the
// actual 802.11 frame. For sniffer_buf (len==128) the 802.11 head is
// truncated to 112 bytes; for raw RxControl-only deliveries (len==12)
// there is no frame data at all.
static constexpr uint16_t RX_CTRL_LEN = 12;

ParsedFrame* FrameParser::parse(const uint8_t* raw_buf, const uint16_t raw_len)
{
    if (raw_buf == nullptr || raw_len < RX_CTRL_LEN + 24) return nullptr;
    const uint8_t* buf = raw_buf + RX_CTRL_LEN;
    uint16_t len = raw_len - RX_CTRL_LEN;

    uint32_t offset = 0;
    //control frame data
    uint8_t type = (buf[0] >> 2) & 0x03;
    uint8_t subtype = (buf[0] >> 4) & 0x0F;
    (void)subtype;

    if(type != 2) return nullptr;

    //header offset
    if(offset + 20 > len) return nullptr;
    uint8_t to_ds = buf[1] & 0x01;
    uint8_t from_ds = (buf[1] >> 1) & 0x01;
    if(to_ds && from_ds)
    {
        offset = 30;
    } else {
        offset = 24;
    }
    offset += 8; // LLC/SNAP header before IPv4

    // IPV4 parsing
    if(offset + 20 > len) return nullptr;
    if(((buf[offset] >> 4) & 0x0F)!= 4) return nullptr;

    uint8_t ihl = ((buf[offset]) & 0x0F )* 4 ;
    uint8_t protocol = buf[offset + 9];
    uint32_t src_ip = ((uint32_t)buf[offset+12] << 24) | ((uint32_t)buf[offset+13] << 16) | ((uint32_t)buf[offset+14] << 8)  |  (uint32_t)buf[offset+15];
    uint32_t dst_ip = ((uint32_t)buf[offset + 16] << 24) | ((uint32_t)buf[offset + 17] << 16) | ((uint32_t)buf[offset + 18] << 8)| ((uint32_t)buf[offset + 19]);


    // transport layer
    uint32_t transport_offset = offset + ihl;
    if (transport_offset + 20 > len) return nullptr;
    uint8_t transport_header_len;
    if(protocol == 6) {
        transport_header_len = ((buf[transport_offset + 12]) >> 4) * 4;
    } else if(protocol == 17) {
        transport_header_len = 8;
    } else {
        return nullptr;  // unsupported protocol
    }
    uint16_t src_port = ((uint16_t)buf[transport_offset] << 8) | (uint16_t)buf[transport_offset+1];
    uint16_t dst_port = ((uint16_t)buf[transport_offset+2] << 8) | (uint16_t)buf[transport_offset+3];

    const uint8_t* payload = buf + transport_offset + transport_header_len;
    uint16_t payload_length = (transport_offset + transport_header_len <= len)
        ? (len - transport_offset - transport_header_len) : 0;
    static ParsedFrame result;
    result.dst_ip = dst_ip;
    result.src_ip = src_ip;
    result.protocol = protocol;
    result.payload = payload;
    result.payload_length = payload_length;
    result.src_port = src_port;
    result.dst_port = dst_port;
    return &result;
}