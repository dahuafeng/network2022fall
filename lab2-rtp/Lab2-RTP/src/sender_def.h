#ifndef __SENDER_DEF_H
#define __SENDER_DEF_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "rtp.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>

#define WAITTIME 1e5

#ifdef __cplusplus
extern "C"
{
#endif

  /**
 * @brief 用于建立RTP连接
 * @param receiver_ip receiver的IP地址
 * @param receiver_port receiver的端口
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 **/
  int initSender(const char *receiver_ip, uint16_t receiver_port, uint32_t window_size);

  /**
 * @brief 用于发送数据 
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
  int sendMessage(const char *message);

  /**
 * @brief 用于发送数据 (优化版本的RTP)
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
  int sendMessageOpt(const char *message);

  /**
 * @brief 用于断开RTP连接以及关闭UDP socket
 **/
  void terminateSender();

  typedef struct
  {
    uint32_t p;
    uint32_t cache_size;                  //cache size=window_size;
    rtp_packet_t buffer[MAX_WINDOW_SIZE]; //cacheline;
    char is_acked[MAX_WINDOW_SIZE];       //is acked by recver 设为1;
  } opt_sender_cache;
  typedef struct
  {
    uint32_t p;
    uint32_t cache_size;
    rtp_packet_t buffer[MAX_WINDOW_SIZE];
  } sender_cache;

#ifdef __cplusplus
}
#endif

#endif
