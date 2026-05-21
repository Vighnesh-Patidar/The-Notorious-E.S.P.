#ifndef FILTER_ENGINE_H
#define FILTER_ENGINE_H

#include "c_types.h"
#include "frame_parser.hpp"
#include "osapi.h"

constexpr uint8_t MAX_RULES = 16;
enum class Action: uint8_t{LOG, COUNT, IGNORE};

struct Rule
{
    uint32_t src_ip_mask;
    uint32_t src_ip_match;
    uint16_t dst_port;
    uint8_t protocol;
    Action action;
};


class FilterEngine
{
    static Rule rules[MAX_RULES];
    static uint8_t rule_count;
public:
    static void apply(const ParsedFrame* frame);
    static uint32_t unmatched_count;
    static bool add_rule(Rule rule);

};


#endif