#include "filter_engine.hpp"
#include "osapi.h"
#include "ets_sys.h"
extern "C" int ets_printf(const char* fmt, ...);

Rule FilterEngine::rules[MAX_RULES];
uint8_t FilterEngine::rule_count = 0;
uint32_t FilterEngine::unmatched_count = 0;

bool FilterEngine::add_rule(Rule rule)
{
    if(rule_count >= MAX_RULES)
    {
        return false;
    } else {
        rules[rule_count++] = rule;
    }
    return true;
}

void FilterEngine::apply(const ParsedFrame* frame)
{
    if(frame == nullptr)
    {
        return ;
    }
    for(uint8_t i = 0; i < rule_count; i++)
    {
        const Rule& rule = rules[i];
        bool proto_match= (rule.protocol == 0 || rule.protocol == frame->protocol);
        bool port_match  = (rule.dst_port == 0 || rule.dst_port == frame->dst_port);
        bool ip_match    = ((frame->src_ip & rule.src_ip_mask) == rule.src_ip_match);
        if (proto_match && port_match && ip_match)
        {
            switch (rule.action)
            {
                case Action::LOG:
                    ets_printf("MATCH: src=%08x dst_port=%d proto=%d\n",frame->src_ip, frame->dst_port, frame->protocol);
                    break;
                case Action::COUNT:
                    unmatched_count++;  
                    break;
                case Action::IGNORE:
                    return;
            }
            return; 
        }
    }
    unmatched_count++;
    ets_printf("WARN: unmatched frame src=%08x proto=%d\n",
    frame->src_ip, frame->protocol);
}