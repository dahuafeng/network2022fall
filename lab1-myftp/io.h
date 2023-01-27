#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>

void panic(char *err)
{
    printf("panic():\n");
    perror(err);
    while (1)
        ;
}

void read_command(char *buffer, int size)
{
    char *nl = NULL;
    memset(buffer, 0, size);

    if (fgets(buffer, size, stdin))
    {
        nl = strchr(buffer, '\n');
        if (nl)
            *nl = '\0';
    }
}

ssize_t Send(int sockfd, const void *buf, size_t n, int flag)
{
    ssize_t ret = 0;
    while (ret < n)
    {
        ssize_t b = send(sockfd, buf + ret, n - ret, flag);
        if (b == 0)
        {
            printf("sockfd closed\n");
            return 0;
        }
        if (b < 0)
        {
            perror("send");
            return -1;
        }
        ret = ret + b;
    }
    return ret;
}

ssize_t Recv(int sockfd, void *buf, size_t n, int flag)
{
    ssize_t ret = 0;
    while (ret < n)
    {
        ssize_t b = recv(sockfd, buf + ret, n - ret, flag);
        if (b == 0)
        {
            printf("sockfd closed\n");
            return 0;
        }
        if (b < 0)
        {
            perror("recv");
            return -1;
        }
        ret = ret + b;
    }
    return ret;
}