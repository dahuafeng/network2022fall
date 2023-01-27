#include "header.h"

int open_listenfd(char *hostname, char *port)
{
    struct addrinfo hints, *listp, *p;
    int listenfd, optval = 1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_flags |= AI_NUMERICSERV;
    if (getaddrinfo(hostname, port, &hints, &listp) != 0)
        panic("getaddrinfo error");

    for (p = listp; p; p = p->ai_next)
    {
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;

        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) == -1)
            panic("setsockopt error");

        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break;

        close(listenfd);
    }

    freeaddrinfo(listp);

    if (!p)
        return -1;

    if (listen(listenfd, LISTENQ) < 0)
    {
        close(listenfd);
        return -1;
    }
    return listenfd;
}

int Open(int connfd)
{
    //printf("recv open request\n");
    fflush(stdout);
    struct Header open_reply;
    open_reply = get_header(1, OPEN_CONN_REPLY, 0);
    if (Send(connfd, &open_reply, sizeof(open_reply), 0) == -1)
        panic("send open reply error");

    return 1;
}

int Auth(int connfd, struct Header auth_request)
{
    //printf("recv auth request\n");
    fflush(stdout);
    char buffer[BUFSIZE];
    uint32_t length = get_length(auth_request);
    memset(buffer, 0, sizeof(buffer));

    char status = 1;
    const char authstr[12] = "user 123123\0";
    const uint32_t authlen = strlen(authstr) + 1;
    int idx = 0;
    while (length)
    {
        size_t recv_length = length < BUFSIZE ? length : BUFSIZE;
        ssize_t b = Recv(connfd, buffer, recv_length, 0);
        if (b < 0)
            panic("recv authstr error");
        length = length - b;
        if (b > authlen || status == 0)
        {
            status = 0;
            continue;
        }
        for (int i = 0; i < b; ++i)
        {

            if (idx >= authlen || buffer[i] != authstr[idx++])
            {
                status = 0;
                break;
            }
        }
        memset(buffer, 0, sizeof(buffer));
    }

    struct Header auth_reply;
    auth_reply = get_header(status, AUTH_REPLY, 0);
    if (Send(connfd, &auth_reply, sizeof(auth_reply), 0) == -1)
        panic("send open reply error");

    return 1;
}

int List(int connfd)
{
    //printf("recv list request\n");
    size_t size;
    char data[BUFSIZE];

    FILE *fd = popen("ls", "r");
    if (fd == NULL)
        panic("ls command error");

    uint32_t length;

    memset(data, 0, sizeof(data));
    size = fread(data, 1, BUFSIZE, fd);

    struct Header put_reply;
    put_reply = get_header(UNUSED, LIST_REPLY, size + 1);
    if (Send(connfd, &put_reply, sizeof(put_reply), 0) == -1)
    {
        panic("send put reply error");
        return 0;
    }

    if (Send(connfd, data, size, 0) == -1)
    {
        panic("send list error\n");
        return 0;
    }

    char c = '\0';
    if (Send(connfd, &c, 1, 0) == -1)
    {
        panic("send list error\n");
        return 0;
    }
    pclose(fd);

    return 1;
}

int Get(int connfd, struct Header get_request)
{
    //printf("recv get request\n");
    size_t size;
    char buffer[BUFSIZE];
    memset(buffer, 0, sizeof(buffer));
    if (Recv(connfd, buffer, get_length(get_request), 0) == -1)
        panic("recv get file_name error");

    char status = 1;

    FILE *fd = fopen(buffer, "r");
    if (fd == NULL)
        status = 0;

    struct Header get_reply;
    get_reply = get_header(status, GET_REPLY, 0);
    if (Send(connfd, &get_reply, sizeof(get_reply), 0) == -1)
        panic("send get reply error");

    if (status == 0)
        return 0;

    uint32_t length;
    struct stat statbuf;
    fstat(fileno(fd), &statbuf);

    length = statbuf.st_size;

    struct Header file_data;
    file_data = get_header(UNUSED, FILE_DATA, length);
    if (Send(connfd, &file_data, sizeof(file_data), 0) == -1)
        panic("send file data error");
    do
    {
        memset(buffer, 0, sizeof(buffer));
        size = fread(buffer, 1, BUFSIZE, fd);
        if (size < 0)
        {
            panic("read wrong file");
            return 0;
        }
        if (size == 0)
            break;

        if (Send(connfd, buffer, size, 0) == -1)
        {
            panic("send put file error\n");
            return 0;
        }
    } while (size > 0);
    fclose(fd);

    return 1;
}

int Put(int connfd, struct Header put_request)
{
    //printf("recv put request\n");
    char buffer[BUFSIZE];
    memset(buffer, 0, sizeof(buffer));
    if (Recv(connfd, buffer, get_length(put_request), 0) == -1)
        panic("recv put file_name error");

    struct Header put_reply;
    put_reply = get_header(UNUSED, PUT_REPLY, 0);
    if (Send(connfd, &put_reply, sizeof(put_reply), 0) == -1)
        panic("send put reply error");

    struct Header file_data;
    if (Recv(connfd, &file_data, sizeof(file_data), 0) == -1)
        panic("recv file data error");

    FILE *fd = fopen(buffer, "w");

    memset(buffer, 0, sizeof(buffer));

    uint32_t file_length = get_length(file_data);

    while (file_length)
    {
        size_t recv_length = file_length < BUFSIZE ? file_length : BUFSIZE;
        ssize_t b = Recv(connfd, buffer, recv_length, 0);
        if (b < 0)
            panic("recv file error");
        file_length = file_length - b;
        fwrite(buffer, 1, b, fd);
        memset(buffer, 0, sizeof(buffer));
    }

    fclose(fd);
    return 1;
}

int Quit(int connfd)
{
    //printf("recv quit request\n");
    struct Header quit_reply;
    quit_reply = get_header(UNUSED, QUIT_REPLY, 0);
    if (Send(connfd, &quit_reply, sizeof(quit_reply), 0) == -1)
        panic("send quit request error");

    return 1;
}
int main(int argc, char **argv)
{

    if (argc != 3)
    {
        printf("Usage:<HOST> <PORT>\n");
        return 0;
    }

    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if ((listenfd = open_listenfd(argv[1], argv[2])) == -1)
    {
        panic("Listen failed\n");
        return 0;
    }
    clientlen = sizeof(struct sockaddr_storage);

server:
    if ((connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) == -1)
    {
        printf("Connection failed\n");
    }

    while (1)
    {

        struct Header request;
        ssize_t b = Recv(connfd, &request, sizeof(request), 0);
        if (b == -1)
            panic("recv request error");
        else if (b == 0)
            goto server;
        switch (request.m_type)
        {
        case OPEN_CONN_REQUEST:
            Open(connfd);
            break;
        case AUTH_REQUEST:
            Auth(connfd, request);
            break;
        case LIST_REQUEST:
            List(connfd);
            break;
        case GET_REQUEST:
            Get(connfd, request);
            break;
        case PUT_REQUEST:
            Put(connfd, request);
            break;
        case QUIT_REQUEST:
            Quit(connfd);
            break;
        default:
            printf("Wrong header type!\n");
            break;
        }
    }

    return 0;
}