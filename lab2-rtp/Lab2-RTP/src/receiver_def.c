#include "receiver_def.h"

static int recvfd;
static struct sockaddr_in cliaddr;
static uint32_t recver_window_size;

static char TIMEOUTFLAG;
static void sigHandler(int signum)
{
    TIMEOUTFLAG = 1;
    ////printf("recvfrom time out\n");
}
static void setTimer(int timeout)
{
    struct itimerval it;

    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = timeout;
    setitimer(ITIMER_REAL, &it, 0);
}
static void resetTimer()
{
    struct itimerval zeroit;
    zeroit.it_interval.tv_sec = 0;
    zeroit.it_interval.tv_usec = 0;
    zeroit.it_value.tv_sec = 0;
    zeroit.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &zeroit, 0);
    TIMEOUTFLAG = 0;
}
static ssize_t Sendto(int __fd, const void *__buf, size_t __n,
                      int __flags, __CONST_SOCKADDR_ARG __addr,
                      socklen_t __addr_len)
{
    ssize_t n;

    n = sendto(__fd, __buf, __n, __flags, __addr, __addr_len);
    if (n == -1)
        perror("recver: sendto error\n");

    return n;
}

//if timeout, return 0;
static ssize_t Recvfrom(int __fd, void *__restrict __buf, size_t __n, int __timeout,
                        __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len)
{

    signal(SIGALRM, sigHandler);
    resetTimer();
    setTimer(__timeout);
    ssize_t n = -1;
    while (n == -1 && TIMEOUTFLAG == 0)
        n = recvfrom(__fd, __buf, __n, MSG_DONTWAIT, __addr, __addr_len);
    if (TIMEOUTFLAG)
    {
        resetTimer();
        return 0;
    }
    resetTimer();
    if (n == -1 && errno != EAGAIN)
    {
        perror("recvfrom error\n");
        return -1;
    }
    return n;
}

static void cache_rtp_pkt(pkt_cache *pcache, rtp_packet_t pkt, uint32_t N)
{

    int place_pos = (pkt.rtp.seq_num - N + pcache->p) % pcache->cache_size;
    pcache->buffer[place_pos] = pkt;
    pcache->valid[place_pos] = 1;
    //printf("recver: cache pkt %u at %d, expect %u\n", pkt.rtp.seq_num, place_pos, N);
}

static uint32_t get_min_ncache(pkt_cache pcache)
{
    ////printf("get min ncache\n");
    uint32_t i;
    for (i = 1; i < pcache.cache_size; ++i)
    {
        if (pcache.valid[(pcache.p + i) % pcache.cache_size] == 0)
            break;
    }
    return pcache.buffer[(pcache.p + i - 1) % pcache.cache_size].rtp.seq_num + 1;
}

/**
 * @brief ??????receiver????????????IP???port????????????????????????
 * 
 * @param port receiver?????????port
 * @param window_size window??????
 * @return -1?????????????????????0??????????????????
 */
int initReceiver(uint16_t port, uint32_t window_size)
{

    //printf("init\n");
    struct sockaddr_in servaddr;

    recvfd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    bind(recvfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    int len = sizeof(cliaddr);
    //printf("recver: bind.\n");
    rtp_packet_t conn_request;
    if (recvfrom(recvfd, &conn_request, RECV_SIZE, 0, (struct sockaddr *)&cliaddr, &len) > 0)
    {

        if (conn_request.rtp.type == RTP_START)
        {

            if (check(conn_request))
            {
                //printf("recver: recv conn_request\n");
                uint32_t start_seq_num = conn_request.rtp.seq_num;
                rtp_packet_t conn_reply;
                set_header(&conn_reply, RTP_ACK, 0, start_seq_num);
                if (Sendto(recvfd, &conn_reply, sizeof(conn_reply), 0, (struct sockaddr *)&cliaddr, len) == -1)
                {
                    perror("recver start sendto error\n");
                    return -1;
                }
                //printf("recver: send conn_reply\n");
                recver_window_size = window_size;

                return 0;
            }
            else
            {
                terminateReceiver();
                return -1;
            }
        }
    }
    perror("recver start recvfrom error\n");
    return -1;
}

/**
 * @brief ??????????????????????????????????????????RTP??????
 * @param filename ??????????????????????????????
 * @return >0?????????????????????????????????????????? -1????????????????????????
 */
int recvMessage(char *filename)
{

    FILE *fp = fopen(filename, "w");
    uint32_t nxt_seq_num = 0;
    uint32_t min_ncached = 1;
    pkt_cache pcache;
    memset(&pcache, 0, sizeof(pcache));
    pcache.p = 0;
    pcache.cache_size = recver_window_size;
    //printf("recver: recv message, window size=%u\n", recver_window_size);
    while (1)
    {
        rtp_packet_t pkt;
        ssize_t recv_n;
        recv_n = Recvfrom(recvfd, &pkt, RECV_SIZE, 1e7, 0, 0);
        if (recv_n == 0)
        {
            terminateReceiver();
            fclose(fp);
            return -1;
        }
        if (recv_n == -1)
        {
            fclose(fp);
            return -1;
        }
        if (recv_n > 0)
        {
            if (check(pkt))
            {
                if (pkt.rtp.type == RTP_DATA)
                {
                    if (pkt.rtp.seq_num < nxt_seq_num + recver_window_size)
                    {
                        if (pkt.rtp.seq_num > nxt_seq_num) //????????????????????????;
                        {
                            //printf("recver: cache pkt %u, expect %u\n", pkt.rtp.seq_num, nxt_seq_num);
                            rtp_packet_t ack;
                            set_header(&ack, RTP_ACK, 0, nxt_seq_num);

                            Sendto(recvfd, &ack, get_ptk_length(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                            cache_rtp_pkt(&pcache, pkt, nxt_seq_num);
                        }
                        else if (pkt.rtp.seq_num == nxt_seq_num)
                        {
                            //printf("recver: cache pkt %u, expect %u\n", pkt.rtp.seq_num, nxt_seq_num);
                            rtp_packet_t ack;
                            cache_rtp_pkt(&pcache, pkt, pkt.rtp.seq_num);

                            nxt_seq_num = get_min_ncache(pcache);

                            set_header(&ack, RTP_ACK, 0, nxt_seq_num);
                            Sendto(recvfd, &ack, get_ptk_length(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));

                            //printf("recver: nxt_seq_num %u\n", nxt_seq_num);
                            uint32_t write_num = nxt_seq_num - pkt.rtp.seq_num;
                            for (int i = 0; i < write_num; ++i)
                            {
                                //printf("recver: write pkt %u\n", pcache.buffer[(pcache.p + i) % pcache.cache_size].rtp.seq_num);
                                fwrite(pcache.buffer[(pcache.p + i) % pcache.cache_size].payload, 1, pcache.buffer[(pcache.p + i) % pcache.cache_size].rtp.length, fp);
                                pcache.valid[(pcache.p + i) % pcache.cache_size] = 0;
                            }
                            pcache.p = (pcache.p + write_num) % pcache.cache_size;
                        }
                    }
                }
                else if (pkt.rtp.type == RTP_END)
                {

                    //printf("recver: end_request recved\n");
                    rtp_packet_t ack;
                    set_header(&ack, RTP_ACK, 0, pkt.rtp.seq_num);
                    Sendto(recvfd, &ack, get_ptk_length(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                    close(recvfd);
                    fclose(fp);
                    //printf("recver: recv msg %s.\n", cbuf);
                    break;
                }
            }
            //else
            //printf("recver: wrong pkt %u,%u recved\n", pkt.rtp.seq_num, pkt.rtp.type);
        }
    }
    return 0;
}
/**
 * @brief ?????????????????????????????????RTP??????????????????UDP socket
 */
void terminateReceiver()
{
    close(recvfd);
}

/**
 * @brief ??????????????????????????????????????????RTP?????? (???????????????RTP)
 * @param filename ??????????????????????????????
 * @return >0?????????????????????????????????????????? -1????????????????????????
 */
int recvMessageOpt(char *filename)
{
    FILE *fp = fopen(filename, "w");
    uint32_t nxt_seq_num = 0;
    uint32_t min_ncached = 1;
    pkt_cache pcache;
    memset(&pcache, 0, sizeof(pcache));
    pcache.p = 0;
    pcache.cache_size = recver_window_size;
    //printf("recver: recv message, window size= %u\n", recver_window_size);
    while (1)
    {
        rtp_packet_t pkt;
        ssize_t recv_n;
        recv_n = Recvfrom(recvfd, &pkt, RECV_SIZE, 1e7, 0, 0);

        if (recv_n == 0)
        {
            terminateReceiver();
            fclose(fp);
            return -1;
        }
        if (recv_n == -1)
        {
            fclose(fp);
            return -1;
        }
        if (recv_n > 0)
        {
            if (check(pkt))
            { //printf("recver: expect %u, recv pkt #%u\n", nxt_seq_num, pkt.rtp.seq_num);
                if (pkt.rtp.type == RTP_DATA)
                {

                    //printf("recver: expect %u, recv pkt #%u\n", nxt_seq_num, pkt.rtp.seq_num);
                    if (pkt.rtp.seq_num < nxt_seq_num + recver_window_size)
                    {
                        if (pkt.rtp.seq_num > nxt_seq_num) //????????????????????????;
                        {

                            rtp_packet_t ack;
                            set_header(&ack, RTP_ACK, 0, pkt.rtp.seq_num);

                            Sendto(recvfd, &ack, get_ptk_length(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                            cache_rtp_pkt(&pcache, pkt, nxt_seq_num);
                            //printf("recver: cache pkt %u\n", pkt.rtp.seq_num);
                        }
                        else if (pkt.rtp.seq_num == nxt_seq_num)
                        {
                            rtp_packet_t ack;
                            cache_rtp_pkt(&pcache, pkt, pkt.rtp.seq_num);
                            nxt_seq_num = get_min_ncache(pcache);
                            //printf("recver: nxt_seq_num %u\n", nxt_seq_num);
                            set_header(&ack, RTP_ACK, 0, pkt.rtp.seq_num);
                            Sendto(recvfd, &ack, get_ptk_length(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));

                            //printf("recver: cache pkt %u\n", pkt.rtp.seq_num);
                            uint32_t write_num = nxt_seq_num - pkt.rtp.seq_num;
                            for (int i = 0; i < write_num; ++i)
                            {
                                //printf("recver: write pkt %u\n", pcache.buffer[(pcache.p + i) % pcache.cache_size].rtp.seq_num);
                                fwrite(pcache.buffer[(pcache.p + i) % pcache.cache_size].payload, 1, pcache.buffer[(pcache.p + i) % pcache.cache_size].rtp.length, fp);
                                pcache.valid[(pcache.p + i) % pcache.cache_size] = 0;
                            }
                            pcache.p = (pcache.p + write_num) % pcache.cache_size;
                        }
                        else if (pkt.rtp.seq_num < nxt_seq_num && pkt.rtp.seq_num + recver_window_size >= nxt_seq_num) //??????pkt, ????????????;
                        {
                            rtp_packet_t ack;
                            set_header(&ack, RTP_ACK, 0, pkt.rtp.seq_num);

                            Sendto(recvfd, &ack, get_ptk_length(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                        }
                        else
                        {
                            perror("recv_opt error: illegal pkt seq num\n");
                            terminateReceiver();
                            fclose(fp);
                            return -1;
                        }
                    }
                }
                else if (pkt.rtp.type == RTP_END)
                {

                    rtp_packet_t ack;
                    set_header(&ack, RTP_ACK, 0, pkt.rtp.seq_num);
                    Sendto(recvfd, &ack, get_ptk_length(ack), 0, (struct sockaddr *)&cliaddr, sizeof(cliaddr));
                    //printf("recver: end_request recved\n");
                    close(recvfd);
                    fclose(fp);

                    break;
                }
            }
        }
    }
    return 0;
}
