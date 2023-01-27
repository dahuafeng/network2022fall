#include "router.h"

int Router::routerNum = 0;
std::map<uint32_t, int> Router::hostTable = {};
std::map<AddrInterval, int> Router::externalNetTable = {};

Router::Router() : id(routerNum)
{
    routerNum++;
}
RouterBase *create_router_object()
{
    return new Router;
}

/*将一个ip地址转换为整数;*/
uint32_t convert_ip_to_int(char *ip)
{

    uint32_t res = 0;
    char *pstr = strtok(ip, ".");
    int tmp;

    for (int i = 0; i != 4 && pstr != NULL; ++i)
    {
        tmp = atoi(pstr);
        pstr = strtok(NULL, ".");

        res += tmp << ((3 - i) * 8);
    }

    return res;
}

/*将一个CIDR表示转换为区间;*/
AddrInterval convert_cidr_to_range(char *cidr)
{
    if (cidr == 0)
        return AddrInterval(0, 0);
    char *ip = strtok(cidr, "/");
    int prelen = atoi(strtok(NULL, "/"));

    uint32_t intIP = convert_ip_to_int(ip);
    AddrInterval res;
    int mask = ((1 << 31) >> (prelen - 1));
    res.first = intIP & (uint32_t)mask;
    res.second = res.first | (~(uint32_t)mask);

    return res;
}

bool inInterval(uint32_t addr, AddrInterval intv)
{
    return (addr >= intv.first && addr <= intv.second);
}

void Router::router_init(int port_num, int external_port, char *external_addr, char *available_addr)
{
    portNum = port_num;
    externalPort = external_port;

    externalAddr = convert_cidr_to_range(external_addr);
    availableAddr = convert_cidr_to_range(available_addr);

    for (uint32_t i = availableAddr.first; i <= availableAddr.second; ++i)
        availableAddrStack.push(i);

    if (external_port)
        externalNetTable[externalAddr] = id;

    DistanceVectorTable = std::vector<std::vector<int>>(portNum + 1, std::vector<int>(128, -1));
    portValueTable = std::vector<int>(portNum + 1, -1);

    DistanceVectorTable[0][id] = 0;
    portValueTable[0] = 0;

    return;
}

int Router::router(int in_port, char *packet)
{
    Header packetHeader;
    memcpy(&packetHeader, packet, sizeof(Header));

    if (packetHeader.type == TYPE_CONTROL)
    {
        char *payload = (char *)malloc(packetHeader.length);
        memset(payload, 0, packetHeader.length);
        memcpy(payload, packet + sizeof(Header), packetHeader.length);

        //TRIGGER DV SEND
        if (payload[0] == '0')
        {

            Header reply;
            reply.src = id;
            reply.dst = 0;
            reply.length = sizeof(int) * DistanceVectorTable[0].size();
            reply.type = TYPE_DV;
            memcpy(packet, &reply, sizeof(Header));
            memcpy(packet + sizeof(Header), &DistanceVectorTable[0][0], reply.length);
            return 0;
        }

        //RELEASE NAT ITEM
        else if (payload[0] == '1')
        {
            char *internal_ip = (char *)malloc(packetHeader.length);
            strcpy(internal_ip, payload + 2);
            auto it = NATTable.find(convert_ip_to_int(internal_ip));
            if (it != NATTable.end())
            {
                availableAddrStack.push(it->second);
                NATTable.erase(it);
            }

            return -1;
        }

        //PORT VALUE CHANGE
        else if (payload[0] == '2')
        {
            char *cmd = strtok(payload, " ");
            int portid = atoi(strtok(NULL, " "));
            int value = atoi(strtok(NULL, " "));

            portValueTable[portid] = value;

            if (portToRouter.count(portid) == 1)
            {

                int minDist = 0x3a3a3a3a;
                for (int i = 2; i <= portNum; ++i)
                {
                    if (portValueTable[i] < 0 || DistanceVectorTable[i][portToRouter[portid]] < 0)
                        continue;
                    if (portValueTable[i] + DistanceVectorTable[i][portToRouter[portid]] < minDist)
                        minDist = portValueTable[i] + DistanceVectorTable[i][portToRouter[portid]];
                }
                if (minDist != 0x3a3a3a3a)
                    DistanceVectorTable[0][portToRouter[portid]] = minDist;
            }
            return -1;
        }

        //ADD HOST
        else if (payload[0] == '3')
        {

            char *cmd = strtok(payload, " ");
            int portid = atoi(strtok(NULL, " "));
            uint32_t ip = convert_ip_to_int(strtok(NULL, " "));

            hostTable[ip] = id;
            hostToPort[ip] = portid;

            return -1;
        }
    }
    else if (packetHeader.type == TYPE_DV)
    {
        memcpy(&DistanceVectorTable[in_port][0], packet + sizeof(Header), packetHeader.length);
        portToRouter[in_port] = packetHeader.src;
        bool changed = false;

        for (int r = 0; r < routerNum; ++r)
        {
            if (r == id)
                continue;
            int minDist = 0x3a3a3a3a;
            for (int i = 2; i <= portNum; ++i)
            {
                if (portValueTable[i] < 0 || DistanceVectorTable[i][r] < 0)
                    continue;
                if (portValueTable[i] + DistanceVectorTable[i][r] < minDist)
                    minDist = portValueTable[i] + DistanceVectorTable[i][r];
            }
            if (minDist == 0x3a3a3a3a)
                continue;
            if (DistanceVectorTable[0][r] != minDist)
                changed = true;

            DistanceVectorTable[0][r] = minDist;
        }

        if (changed)
        {
            Header reply;
            reply.src = id;
            reply.dst = 0;
            reply.length = sizeof(int) * DistanceVectorTable[0].size();
            reply.type = TYPE_DV;
            memcpy(packet, &reply, sizeof(Header));
            memcpy(packet + sizeof(Header), &DistanceVectorTable[0][0], reply.length);
            return 0;
        }

        return -1;
    }
    else if (packetHeader.type == TYPE_DATA)
    {

        if (in_port == externalPort) //从外网来的报文;
        {
            auto it = NATTable.begin();
            for (; it != NATTable.end(); it++)
            {
                if (it->second == ntohl(packetHeader.dst))
                {
                    packetHeader.dst = htonl(it->first);
                    break;
                }
            }
            if (it == NATTable.end())
                return -1;

            memcpy(packet, &packetHeader, sizeof(Header));
        }
        if (inInterval(ntohl(packetHeader.dst), externalAddr)) //发往外网;
        {
            if (NATTable.count(ntohl(packetHeader.src)) == 0)
            {
                //如果没有分配，分配一个;
                if (availableAddrStack.empty())
                    return -1;
                uint32_t external_ip = availableAddrStack.top();
                availableAddrStack.pop();
                NATTable[ntohl(packetHeader.src)] = external_ip;
            }
            packetHeader.src = htonl(NATTable[ntohl(packetHeader.src)]);
            memcpy(packet, &packetHeader, sizeof(Header));
            return externalPort;
        }
        //不需要NAT;

        int destRouter = -1;
        if (hostTable.count(ntohl(packetHeader.dst)))
            destRouter = hostTable[ntohl(packetHeader.dst)];
        else
        {
            for (auto ii = externalNetTable.begin(); ii != externalNetTable.end(); ii++)
            {
                if (inInterval(ntohl(packetHeader.dst), ii->first))
                {
                    destRouter = ii->second;
                    break;
                }
            }
        }
        if (destRouter == id)
            return hostToPort[ntohl(packetHeader.dst)];
        if (destRouter == -1)
            return 1;
        if (DistanceVectorTable[0][destRouter] == -1)
            return 1;
        for (int i = 2; i <= portNum; ++i)
        {
            if (portValueTable[i] == -1 || DistanceVectorTable[i][destRouter] == -1)
                continue;
            if (portValueTable[i] + DistanceVectorTable[i][destRouter] == DistanceVectorTable[0][destRouter])
                return i;
        }
        perror("router\n");
    }

    return 1;
}