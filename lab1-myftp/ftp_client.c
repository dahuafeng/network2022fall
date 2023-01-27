#include "header.h"

int open_clientfd(char *server_ip, char *server_port)
{
    int clientfd;
    struct addrinfo hints, *listp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_flags |= AI_ADDRCONFIG;
    if (getaddrinfo(server_ip, server_port, &hints, &listp) != 0)
        panic("getaddrinfo error");

    struct addrinfo *p;

    for (p = listp; p; p = p->ai_next)
    {
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break; //success;
        close(clientfd);
    }
    freeaddrinfo(listp);
    if (!p)
        return -1;
    else
        return clientfd;
}

//Open a connection with the server. Return clientfd if success, or return -1.
int Open(char *server_ip, char *server_port)
{
    int clientfd;
    if ((clientfd = open_clientfd(server_ip, server_port)) == -1)
    {
        printf("Connection cannot established now.\n");
        return -1;
    }
    struct Header open_request;
    open_request = get_header(UNUSED, OPEN_CONN_REQUEST, 0);
    if (Send(clientfd, &open_request, sizeof(open_request), 0) == -1)
        panic("send open request error");

    struct Header open_reply;
    if (Recv(clientfd, &open_reply, sizeof(open_reply), 0) == -1)
        panic("recv open reply error");
    if (open_reply.m_status == 1)
    {
        printf("Successfully connect with %s:%s\n", server_ip, server_port);
        return clientfd;
    }
    else
    {
        printf("Open has been denied\n");
        return -1;
    }
}

//Authorize the user to connect. Return 1 if success, or return 0.
int Auth(int clientfd, char *authstr)
{
    struct Header auth_request;
    uint32_t length = strlen(authstr) + 1;
    auth_request = get_header(UNUSED, AUTH_REQUEST, length);

    if (Send(clientfd, &auth_request, sizeof(auth_request), 0) == -1)
        panic("send auth request error");
    if (Send(clientfd, authstr, strlen(authstr) + 1, 0) == -1)
        panic("send authstr error");

    struct Header auth_reply;
    if (Recv(clientfd, &auth_reply, sizeof(auth_reply), 0) == -1)
        panic("recv auth reply error");

    if (auth_reply.m_status == 1)
    {
        printf("Welcome!\n");
        return 1;
    }
    else
    {
        printf("Auth failed, connection will be closed soon\n");
        close(clientfd);
        return 0;
    }
}

//List the files of the server. Return 1 if success, or return 0.
int List(int clientfd)
{
    struct Header list_request;
    list_request = get_header(UNUSED, LIST_REQUEST, 0);

    if (Send(clientfd, &list_request, sizeof(list_request), 0) == -1)
        panic("send list request error");

    struct Header list_reply;
    if (Recv(clientfd, &list_reply, sizeof(list_reply), 0) == -1)
        panic("recv list reply error");

    if (list_reply.m_type == LIST_REPLY)
    {
        char buffer[BUFSIZE];

        uint32_t length = get_length(list_reply);

        memset(buffer, 0, sizeof(buffer));

        while (length)
        {
            size_t recv_length = length < BUFSIZE ? length : BUFSIZE;
            ssize_t b = Recv(clientfd, buffer, recv_length, 0);
            if (b < 0)
                panic("recv list error");
            length = length - b;
            printf("%s", buffer);
            memset(buffer, 0, sizeof(buffer));
        }
        return 1;
    }
    else
    {
        printf("List failed\n");
        return 0;
    }
}

//Download file file_name from the server. Return 1 if success, or return 0.
int Get(int clientfd, char *file_name)
{
    struct Header get_request;
    uint32_t length = strlen(file_name) + 1;
    get_request = get_header(UNUSED, GET_REQUEST, length);

    char data[BUFSIZE];
    memset(data, 0, BUFSIZE);
    memcpy(data, file_name, strlen(file_name));

    if (Send(clientfd, &get_request, sizeof(get_request), 0) == -1)
        panic("send get request error");
    if (Send(clientfd, data, strlen(file_name) + 1, 0) == -1)
        panic("send authstr error");

    struct Header get_reply;
    if (Recv(clientfd, &get_reply, sizeof(get_reply), 0) == -1)
        panic("recv get reply error");

    if (get_reply.m_type == GET_REPLY)
    {
        if (get_reply.m_status == 1)
        {
            struct Header file_data;
            if (Recv(clientfd, &file_data, sizeof(file_data), 0) == -1)
                panic("recv get reply error");

            FILE *fd = fopen(file_name, "w");

            char buffer[BUFSIZE];
            memset(buffer, 0, sizeof(buffer));

            uint32_t file_length = get_length(file_data);

            while (file_length)
            {
                size_t recv_length = file_length < BUFSIZE ? file_length : BUFSIZE;
                ssize_t b = Recv(clientfd, buffer, recv_length, 0);
                if (b < 0)
                    panic("recv file error");
                file_length = file_length - b;
                fwrite(buffer, 1, b, fd);
                memset(buffer, 0, sizeof(buffer));
            }

            fclose(fd);
        }
        else
        {
            printf("GET ERROR: File doesn't exist!\n");
            return 0;
        }
    }
    else
    {
        printf("Get failed\n");
        return 0;
    }
    printf("File downloaded.\n");
    return 1;
}

//Upload file file_name to the server. Return 1 if success, or return 0.
int Put(int clientfd, char *file_name)
{
    size_t size;
    char data[BUFSIZE];

    FILE *fd = fopen(file_name, "r");
    if (fd == NULL)
    {
        printf("PUT ERROR: File doesn't exist!\n");
        return 0;
    }
    struct Header put_request, file_data;
    uint32_t length;

    struct stat statbuf;
    fstat(fileno(fd), &statbuf);

    length = statbuf.st_size;

    put_request = get_header(UNUSED, PUT_REQUEST, strlen(file_name) + 1);
    file_data = get_header(UNUSED, FILE_DATA, length);
    if (Send(clientfd, &put_request, sizeof(put_request), 0) == -1)
        panic("send put request error");
    if (Send(clientfd, file_name, strlen(file_name) + 1, 0) == -1)
        panic("send put request error");

    struct Header put_reply;

    if (Recv(clientfd, &put_reply, sizeof(put_reply), 0) == -1)
    {
        panic("recv put reply error");
        return 0;
    }

    if (put_reply.m_type == PUT_REPLY)
    {
        if (Send(clientfd, &file_data, sizeof(file_data), 0) == -1)
            panic("send file data error");
    }
    do
    {
        memset(data, 0, sizeof(data));
        size = fread(data, 1, BUFSIZE, fd);
        if (size < 0)
        {
            panic("put wrong file");
            return 0;
        }
        if (size == 0)
            break;

        if (Send(clientfd, data, size, 0) == -1)
        {
            panic("send put file error\n");
            return 0;
        }
    } while (size > 0);
    fclose(fd);
    printf("File uploaded.\n");
    return 1;
}

//Close the connection.
int Quit(int clientfd)
{
    struct Header quit_request;
    quit_request = get_header(UNUSED, QUIT_REQUEST, 0);
    if (Send(clientfd, &quit_request, sizeof(quit_request), 0) == -1)
        panic("send quit request error");

    struct Header quit_reply;
    if (Recv(clientfd, &quit_reply, sizeof(quit_reply), 0) == -1)
        panic("recv quit reply error");
    if (quit_reply.m_type == QUIT_REPLY)
    {
        printf("Connection will be closed soon. Goodbye!\n");
        return 1;
    }
    else
    {
        printf("Quit failed\n");
        return 0;
    }
}

int main()
{
    char buffer[BUFSIZE];
    int clientfd, auth;

    auth = 0;
    clientfd = -1;

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        printf("Clinet> ");
        fflush(stdout);

        read_command(buffer, sizeof(buffer));

        char *cmd;
        cmd = strtok(buffer, " ");

        char *arg1, *arg2;
        arg1 = strtok(NULL, " ");
        arg2 = strtok(NULL, " ");

        if (!strcmp(cmd, "open"))
        {
            if (clientfd != -1)
                printf("Connection established before has not been closed yet.\n");
            else
                clientfd = Open(arg1, arg2);
        }
        else
        {
            if (clientfd == -1)
            {
                printf("Connection has not been established yet!\n");
                continue;
            }
            if (!strcmp(cmd, "auth"))
            {
                char authstr[BUFSIZE];
                strcpy(authstr, arg1);
                int p = strlen(authstr);
                authstr[p] = ' ';
                strcpy(authstr + p + 1, arg2);

                if (Auth(clientfd, authstr))
                    auth = 1;
                else
                    break;
            }

            else
            {
                if (auth == 0)
                {
                    printf("Please login.\n");
                    continue;
                }
                if (!strcmp(cmd, "ls"))
                {
                    List(clientfd);
                }
                else if (!strcmp(cmd, "get"))
                {
                    Get(clientfd, arg1);
                }
                else if (!strcmp(cmd, "put"))
                {
                    Put(clientfd, arg1);
                }
                else if (!strcmp(cmd, "quit"))
                {
                    Quit(clientfd);
                    close(clientfd);
                    break;
                }
                else
                    printf("Wrong command!\n");
            }
        }
    }
    return 0;
}