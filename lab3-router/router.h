#include "router_prototype.h"

class Router : public RouterBase
{
    /*NAT表, {internal_ip: external_ip}*/
    std::unordered_map<uint32_t, uint32_t> NATTable;

    /*距离向量表, 0 for self*/
    std::vector<std::vector<int>> DistanceVectorTable;

    /*{portid: routerid}*/
    std::map<int, int> portToRouter;

    std::vector<int> portValueTable;

    /*路由器端口数*/
    int portNum;

    /*外网端口号，0表示不连接外网*/
    int externalPort;

    /*连接的外网地址范围*/
    AddrInterval externalAddr;

    /*路由器可用公网地址范围*/
    AddrInterval availableAddr;

    /*路由器编号*/
    int id;

    /*{host ip: port}*/
    std::map<uint32_t, int> hostToPort;

    /*restore the available addr*/
    std::stack<uint32_t> availableAddrStack;

public:
    static int routerNum;
    static std::map<uint32_t, int> hostTable;
    static std::map<AddrInterval, int> externalNetTable;

    Router();
    void router_init(int port_num, int external_port, char *external_addr, char *available_addr);
    int router(int in_port, char *packet);
};
