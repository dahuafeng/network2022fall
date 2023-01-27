#include "rtp.h"
void set_header(rtp_packet_t *pkt, uint8_t __type, uint16_t __length, uint32_t __seq_num)
{
    pkt->rtp.type = __type;
    pkt->rtp.length = __length;
    pkt->rtp.seq_num = __seq_num;
    pkt->rtp.checksum = 0;
    pkt->rtp.checksum = compute_checksum(pkt, get_ptk_length(*pkt));
}

uint16_t get_ptk_length(rtp_packet_t pkt)
{
    return pkt.rtp.length + 11;
}

int check(rtp_packet_t pkt)
{
    uint32_t pkt_checksum = pkt.rtp.checksum;
    pkt.rtp.checksum = 0;
    return (pkt_checksum == compute_checksum(&pkt, get_ptk_length(pkt)));
}