#include "sender_def.h"

static int connfd;
static uint32_t sender_window_size;
static uint32_t send_base;
static uint32_t nxt_seq_num;

static struct sockaddr_in recvaddr;

static int TIMEOUT;
static void sigHandler(int signum)
{
    TIMEOUT = 1;
    //printf("recvfrom time out\n");
}
static void setTimer()
{
    signal(SIGALRM, sigHandler);
    struct itimerval it;

    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 0;
    it.it_value.tv_sec = 0;
    it.it_value.tv_usec = WAITTIME;
    setitimer(ITIMER_REAL, &it, 0);
    TIMEOUT = 0;
}
static void resetTimer()
{
    signal(SIGALRM, sigHandler);
    struct itimerval zeroit;
    zeroit.it_interval.tv_sec = 0;
    zeroit.it_interval.tv_usec = 0;
    zeroit.it_value.tv_sec = 0;
    zeroit.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &zeroit, 0);
    TIMEOUT = 0;
}
static ssize_t Sendto(int __fd, const void *__buf, size_t __n,
                      int __flags, __CONST_SOCKADDR_ARG __addr,
                      socklen_t __addr_len)
{
    ssize_t n;

    n = sendto(__fd, __buf, __n, __flags, __addr, __addr_len);
    if (n == -1)
        perror("sender: sendto error\n");

    return n;
}

//if recv nothing, return 0;
static ssize_t Recv(int __fd, void *__restrict __buf, size_t __n)
{

    ssize_t n;

    n = recvfrom(__fd, __buf, __n, MSG_DONTWAIT, 0, 0);
    if (n == -1 && errno != EAGAIN)
    {
        perror("recvfrom error\n");
        return -1;
    }
    if (n == -1 && errno == EAGAIN)
        return 0;

    return n;
}

static void cachePkt(sender_cache *pcache, rtp_packet_t pkt, uint32_t base)
{
    int place_pos = (pkt.rtp.seq_num - base + pcache->p) % pcache->cache_size;
    pcache->buffer[place_pos] = pkt;
}
/**
 * @brief 用于建立RTP连接
 * @param receiver_ip receiver的IP地址
 * @param receiver_port receiver的端口
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 **/
int initSender(const char *receiver_ip, uint16_t receiver_port, uint32_t window_size)
{

    if ((connfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket created error\n");
        return -1;
    }

    bzero(&recvaddr, sizeof(recvaddr));

    recvaddr.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &recvaddr.sin_addr);
    recvaddr.sin_port = htons(receiver_port);

    rtp_packet_t conn_request;
    set_header(&conn_request, RTP_START, 0, 0);

    if (Sendto(connfd, &conn_request, get_ptk_length(conn_request), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr)) == -1)
    {
        return -1;
    }
    //printf("sender: conn_request sent\n");
    rtp_packet_t conn_reply;
    ssize_t recv_n = 0;
    setTimer();
    while (recv_n == 0 && !TIMEOUT)
        recv_n = Recv(connfd, &conn_reply, RECV_SIZE);
    resetTimer();
    if (recv_n > 0)
    {
        if (conn_reply.rtp.type == RTP_ACK && conn_reply.rtp.seq_num == 0)
        {
            if (check(conn_reply))
            {
                //printf("sender: conn_reply recved.\n");
                sender_window_size = window_size;
                send_base = 0;
                return 0;
            }
            //printf("sender: conn_reply broken\n");
            rtp_packet_t end_request;
            set_header(&end_request, RTP_END, 0, send_base);

            Sendto(connfd, &end_request, get_ptk_length(end_request), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr));
            return -1;
        }
    }
    else if (recv_n == 0)
    {
        //printf("sender: conn_reply timeout\n");
        rtp_packet_t end_request;
        set_header(&end_request, RTP_END, 0, send_base);

        Sendto(connfd, &end_request, get_ptk_length(end_request), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr));
        return -1;
    }
    //printf("connet error\n");
    return -1;
}

/**
 * @brief 用于发送数据 
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessage(const char *message)
{
    send_base = 0;
    nxt_seq_num = 0;
    int sent = 0;

    int un_acked_pkt = 0;

    FILE *fp = fopen(message, "r");
    if (fp == 0)
    {
        perror("sender: file doesn't exist\n");
        return -1;
    }
    struct stat stat_buf;
    stat(message, &stat_buf);

    uint32_t msg_len = stat_buf.st_size;

    sender_cache pcache;
    memset(&pcache, 0, sizeof(pcache));
    pcache.p = 0;
    pcache.cache_size = sender_window_size;
    //printf("sender: send message, window size= %u,msg length= %u\n", sender_window_size, msg_len);
    setTimer();
    while (sent < msg_len)
    {
        if (nxt_seq_num < sender_window_size + send_base)
        {
            rtp_packet_t pkt;
            uint16_t payload_length = fread(pkt.payload, 1, PAYLOAD_SIZE, fp);
            set_header(&pkt, RTP_DATA, payload_length, nxt_seq_num);

            if (Sendto(connfd, &pkt, get_ptk_length(pkt), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr)) == -1)
            {
                fclose(fp);
                return -1;
            }
            //printf("sender: send pkt %u\n", nxt_seq_num);
            nxt_seq_num++;
            cachePkt(&pcache, pkt, send_base);
        }
        rtp_packet_t ack;
        ssize_t recv_n;
        recv_n = Recv(connfd, &ack, RECV_SIZE);
        if (recv_n == -1)
        {
            fclose(fp);
            return -1;
        }
        if (recv_n > 0)
        {
            if (ack.rtp.type == RTP_ACK)
            {
                if (check(ack))
                {
                    //printf("sender: recv ack # %u, send base %u\n", ack.rtp.seq_num, send_base);
                    if (ack.rtp.seq_num > send_base)
                    {
                        int acked_pkt = ack.rtp.seq_num - send_base;
                        send_base = ack.rtp.seq_num;
                        //printf("sender: send base changes to %u\n", send_base);
                        pcache.p = (pcache.p + acked_pkt) % pcache.cache_size;
                        for (int j = 0; j < acked_pkt; ++j)
                        {
                            if (msg_len - sent < PAYLOAD_SIZE)
                                sent += msg_len - sent;
                            else
                                sent += PAYLOAD_SIZE;
                        }
                        setTimer();
                    }
                }
            }
        }
        else if (TIMEOUT) //重传报文;
        {
            uint32_t resend_num = nxt_seq_num - send_base;
            for (int ii = 0; ii < resend_num; ++ii)
            {
                uint32_t pkt_seq = pcache.buffer[(pcache.p + ii) % pcache.cache_size].rtp.seq_num;
                if (pkt_seq < nxt_seq_num && pkt_seq >= send_base)
                {
                    if (Sendto(connfd, &pcache.buffer[(pcache.p + ii) % pcache.cache_size], get_ptk_length(pcache.buffer[(pcache.p + ii) % pcache.cache_size]), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr)) == -1)
                    {
                        fclose(fp);
                        return -1;
                    }
                    //printf("sender: resend pkt %u\n", pkt_seq);
                }
            }
        }
    }
    fclose(fp);
    return 0;
}

void terminateSender()
{

    rtp_packet_t end_request;
    set_header(&end_request, RTP_END, 0, send_base);

    Sendto(connfd, &end_request, get_ptk_length(end_request), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr));
    //printf("sender: send end request\n");
    rtp_packet_t end_reply;
    ssize_t recv_n;
    setTimer();
    while (recv_n == 0 && !TIMEOUT)
        recv_n = Recv(connfd, &end_reply, RECV_SIZE);
    resetTimer();
    if (recv_n > 0)
    {
        if (end_reply.rtp.type == RTP_ACK && end_reply.rtp.seq_num == send_base)
        {
            //printf("sender: recv end ack\n");
            close(connfd);
            return;
        }
    }
    if (recv_n == 0)
        close(connfd);
}

/**
 * @brief 用于发送数据 (优化版本的RTP)
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessageOpt(const char *message)
{

    send_base = 0;
    nxt_seq_num = 0;
    int sent = 0;

    FILE *fp = fopen(message, "r");
    if (fp == 0)
    {
        perror("sender: file doesn't exist\n");
        return -1;
    }
    struct stat stat_buf;
    stat(message, &stat_buf);

    uint32_t msg_len = stat_buf.st_size;
    opt_sender_cache pcache;
    memset(&pcache, 0, sizeof(pcache));
    pcache.cache_size = sender_window_size;
    pcache.p = 0;
    //printf("sender: send message, window size= %u,msg length= %u\n", sender_window_size, msg_len);
    setTimer();
    while (sent < msg_len)
    {

        if (nxt_seq_num < sender_window_size + send_base)
        {
            rtp_packet_t pkt;
            uint16_t payload_length = fread(pkt.payload, 1, PAYLOAD_SIZE, fp);
            set_header(&pkt, RTP_DATA, payload_length, nxt_seq_num);

            pcache.buffer[nxt_seq_num % pcache.cache_size] = pkt;
            pcache.is_acked[nxt_seq_num % pcache.cache_size] = 0;

            if (Sendto(connfd, &pkt, get_ptk_length(pkt), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr)) == -1)
            {
                fclose(fp);
                return -1;
            }

            //printf("sender: send pkt %u\n", nxt_seq_num);
            nxt_seq_num++;
        }

        rtp_packet_t ack;
        ssize_t recv_n;
        recv_n = Recv(connfd, &ack, RECV_SIZE);
        if (recv_n == -1)
        {
            fclose(fp);
            return -1;
        }
        else if (recv_n > 0)
        {

            if (ack.rtp.type == RTP_ACK)
            {
                if (check(ack))
                {
                    //printf("sender: recv ack #%u, send base %u\n", ack.rtp.seq_num, send_base);

                    if (ack.rtp.seq_num >= send_base && ack.rtp.seq_num < sender_window_size + send_base)
                    {

                        if (pcache.is_acked[ack.rtp.seq_num % pcache.cache_size] == 0)
                        {
                            pcache.is_acked[ack.rtp.seq_num % pcache.cache_size] = 1;
                            sent += pcache.buffer[ack.rtp.seq_num % pcache.cache_size].rtp.length;
                            if (ack.rtp.seq_num == send_base)
                            {
                                for (int ii = 0; ii < pcache.cache_size; ++ii)
                                {
                                    if (pcache.is_acked[(pcache.p + ii) % pcache.cache_size] == 1)
                                        send_base++;
                                    else
                                        break;
                                }
                                pcache.p = send_base % pcache.cache_size;

                                setTimer();
                            }
                        }
                    }
                }
            }
        }

        else if (TIMEOUT)
        {
            uint32_t resend_num = nxt_seq_num - send_base;
            for (int ii = 0; ii < resend_num; ++ii)
            {
                if (pcache.is_acked[(ii + pcache.p) % pcache.cache_size] == 0)
                {
                    uint32_t pkt_seq = pcache.buffer[(pcache.p + ii) % pcache.cache_size].rtp.seq_num;
                    if (pkt_seq < nxt_seq_num + send_base && pkt_seq >= send_base)
                    {
                        //printf("sender: resend pkt %u\n", pkt_seq);
                        if (Sendto(connfd, &pcache.buffer[(pcache.p + ii) % pcache.cache_size], get_ptk_length(pcache.buffer[(pcache.p + ii) % pcache.cache_size]), 0, (struct sockaddr *)&recvaddr, sizeof(recvaddr)) == -1)
                        {
                            fclose(fp);
                            return -1;
                        }
                    }
                }
            }
        }
    }
    fclose(fp);
    return 0;
}
