#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include "util.h"
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define RTP_START 0
#define RTP_END 1
#define RTP_DATA 2
#define RTP_ACK 3

#define PAYLOAD_SIZE 1461
#define RECV_SIZE 1501

#define MAX_WINDOW_SIZE 1024

    typedef struct __attribute__((__packed__)) RTP_header
    {
        uint8_t type;    // 0: START; 1: END; 2: DATA; 3: ACK
        uint16_t length; // Length of data; 0 for ACK, START and END packets
        uint32_t seq_num;
        uint32_t checksum; // 32-bit CRC
    } rtp_header_t;

    typedef struct __attribute__((__packed__)) RTP_packet
    {
        rtp_header_t rtp;
        char payload[PAYLOAD_SIZE];
    } rtp_packet_t;

    void set_header(rtp_packet_t *pkt, uint8_t __type, uint16_t __length, uint32_t __seq_num);

    uint16_t get_ptk_length(rtp_packet_t pkt);

    int check(rtp_packet_t pkt);

#ifdef __cplusplus
}
#endif

#endif //RTP_H
