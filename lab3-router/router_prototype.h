#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <arpa/inet.h>
#include <iterator>
#include <stack>
#include <assert.h>

#define TYPE_DV 0x00
#define TYPE_DATA 0x01
#define TYPE_CONTROL 0x02

typedef std::pair<uint32_t, uint32_t> AddrInterval;

/*
    src 和 dst 字段分别表示源和目的 IPv4 地址。
    length 字段为报头后的 payload 长度。
    type 字段有三个取值，分别为 TYPE_DV , TYPE_DATA 和 TYPE_CONTROL。
    src 与 dst 字段为大端法表示，其余字段均为小端法表示。
*/
struct Header
{
    uint32_t src;
    uint32_t dst;
    uint8_t type;
    uint16_t length;
};

class RouterBase
{
public:
    virtual void router_init(int port_num, int external_port, char *external_addr, char *available_addr) = 0;

    /* return out_port, 0 = broadcast, -1 = drop, 1 = default */
    virtual int router(int in_port, char *packet) = 0;
};

RouterBase *create_router_object();