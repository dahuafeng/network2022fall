

#include "io.h"

#define MAGIC_NUMBER_LENGTH 6
#define BUFSIZE 2048
#define LISTENQ 1024

#define SIZE(length) length + 12

#define OPEN_CONN_REQUEST (char)0xa1
#define OPEN_CONN_REPLY (char)0xa2
#define AUTH_REQUEST (char)0xa3
#define AUTH_REPLY (char)0xa4
#define LIST_REQUEST (char)0xa5
#define LIST_REPLY (char)0xa6
#define GET_REQUEST (char)0xa7
#define GET_REPLY (char)0xa8
#define PUT_REQUEST (char)0xa9
#define PUT_REPLY (char)0xaa
#define QUIT_REQUEST (char)0xab
#define QUIT_REPLY (char)0xac

#define FILE_DATA (char)0xff

#define UNUSED (char)3

struct Header
{
    char m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    char m_type;                          /* type (1 byte) */
    char m_status;                        /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
    ;
} __attribute__((packed));

struct Header get_header(char m_status, char m_type, uint32_t m_length)
{
    struct Header header;
    memcpy(header.m_protocol, "\xe3myftp", sizeof(header.m_protocol));
    header.m_status = m_status;
    header.m_type = m_type;
    header.m_length = htonl(m_length + 12);

    return header;
}

uint32_t get_length(struct Header header)
{
    return ntohl(header.m_length) - 12;
}
