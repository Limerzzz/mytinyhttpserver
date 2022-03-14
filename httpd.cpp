/*
 * @Author: Limer
 * @Date: 2022-03-04 13:09:31
 * @LastEditors: Limer
 * @LastEditTime: 2022-03-14 13:04:30
 * @Description: This is a  program of tiny http server.
 */
#include <arpa/inet.h>
#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
// when it comes to compile, you need to add '-lpthread'.
#include <thread>

#define BUF_SIZE 1024
#define METHOD_SIZE 255
#define PATH_SIZE 512

#define SERVER_STRING "Server: Limer's http/0.1.0\r\n"

int get_line(int sockfd, char* buf, size_t buf_size);
void serve_file(int client_sockfd, const char* filename);
void not_found(int client_sockfd);
void headers(int client_sockfd, const char* filename);
void execute_cgi(int client, const char* path, const char* method,
                 const char* query_string);
void bad_request(int client);
void cannot_execute(int client);
void cannot_execute(int client) {
    char buf[1024];
    //发送500
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}
void bad_request(int client) {
    char buf[1024];
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}
void execute_cgi(int client, const char* path, const char* method,
                 const char* query_string) {
    char buf[BUF_SIZE];
    int cgi_output[2];
    int cgi_input[2];

    pid_t pid;
    int status;
    int i;
    char c;

    int numchars    = 1;
    int content_len = -1;

    // default chars
    buf[0] = 'A';
    buf[1] = '\0';
    if(strcasecmp(method, "GET") == 0) {
        while((numchars > 0) && strcmp("\n", buf)) {
            numchars = get_line(client, buf, sizeof(buf));
        }
    }
    else {
        numchars = get_line(client, buf, sizeof(buf));
        while((numchars > 0) && strcmp("\n", buf)) {
            buf[15] = '\0';
            if(strcasecmp(buf, "Content-Length:") == 0) {
                content_len = atoi(&(buf[16]));
            }
            numchars = get_line(client, buf, sizeof(buf));
        }
        if(content_len == -1) {
            bad_request(client);
            return;
        }
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    // initialize pipe
    if(pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if(pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if((pid = fork()) < 0) {
        cannot_execute(client);
        return;
    }
    // subprocess
    if(pid == 0) {
        char meth_env[255];
        char query_env[255];
        char length_env[255];
        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);

        close(cgi_output[0]);
        close(cgi_input[1]);

        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        // TODO putenv()????
        putenv(meth_env);

        if(strcasecmp(method, "GET") == 0) {
            sprintf(length_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {
            sprintf(length_env, "CONTENT_LENGTH=%d", content_len);
        }
        execl(path, path, NULL);
        exit(0);
    }
    else {
        close(cgi_output[1]);
        close(cgi_input[0]);
        if(strcasecmp(method, "POST") == 0) {
            for(i = 0; i < content_len; ++i) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }
        while(read(cgi_output[0], &c, 1) > 0) {
            send(client, &c, 1, 0);
        }
        close(cgi_output[0]);
        close(cgi_input[1]);

        waitpid(pid, &status, 0);
    }
}
void cat(int client, FILE* resource) {
    //发送文件的内容
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource)) {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}
void headers(int client_sockfd, const char* filename) {
    char buf[1024];
    (void)filename; /* could use filename to determine file type */
                    //发送HTTP头
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
}
// return 404 error page
void not_found(int client_sockfd) {
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client_sockfd, buf, strlen(buf), 0);
}
void serve_file(int client_sockfd, const char* filename) {
    FILE* resource = nullptr;
    int numchars   = 1;
    char buf[BUF_SIZE];
    buf[0] = 'A';
    buf[1] = '\0';
    while((numchars > 0) && strcmp("\n", buf)) {
        numchars = get_line(client_sockfd, buf, sizeof(buf));
    }

    // open file
    resource = fopen(filename, "r");
    if(resource == nullptr) {
        not_found(client_sockfd);
    }
    else {
        headers(client_sockfd, filename);
        cat(client_sockfd, resource);
    }
    fclose(resource);
}

void error_die(const std::string& str) {
    perror(str.c_str());
    exit(1);
}
/**
 * @description: create a socket, bind a sockaddr and listen the socket. 
 * @param {int&} port, the listen port
 * @return {int} return the sockfd
 */
int startup(int& port) {
    int httpd = 0, option;
    struct sockaddr_in serv_addr;
    // set http socket
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    // fail to connect
    if(httpd == -1)
        error_die("socket");

    socklen_t optlen;
    optlen = sizeof(option);
    option = 1;
    setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, static_cast< void* >(&option),
               optlen);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_port        = htons(port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(httpd, (sockaddr*)(&serv_addr), sizeof(serv_addr)) < 0) {
        error_die("bind");
    }
    if(port == 0) {  // dynamicly assign a port
        socklen_t addrlen = sizeof(serv_addr);
        if(getsockname(httpd, (struct sockaddr*)&serv_addr, &addrlen) == -1)
            error_die("getsockname");
    }
    if(listen(httpd, 5) < 0)
        error_die("listen");
    return httpd;
}
// parse a line http message
int get_line(int sockfd, char* buf, size_t buf_size) {
    int i  = 0;
    char c = '\0';
    int n;
    while((i < buf_size - 1) && (c != '\n')) {
        // get a char from buffer of client fd
        n = recv(sockfd, &c, 1, 0);
        if(n > 0) {
            // '\r' == /
            if(c == '\r') {
                n = recv(sockfd, &c, 1, MSG_PEEK);
                if((n > 0) && (c == '\n'))
                    recv(sockfd, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            ++i;
        }
        else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}

void unimplemented(int sockfd) {
    char buf[1024];
    // sent 501 to indicate that this method is unimplement.
    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(sockfd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(sockfd, buf, strlen(buf), 0);
}
// handle the http request
void* accept_request(void* from_client) {

    int client_sockfd = *(int*)from_client;
    char buf[BUF_SIZE];
    int numchars;
    // TODO using std::string to replace the char array
    char method[METHOD_SIZE];
    char url[METHOD_SIZE];
    char path[PATH_SIZE];
    size_t i, j;
    struct stat st;
    int cgi            = 0;
    char* query_string = nullptr;

    numchars = get_line(client_sockfd, buf, sizeof(buf));

    i = 0, j = 0;
    while(buf[i] != ' ' && i < METHOD_SIZE) {
        // extract the method string of buf
        method[i] = buf[j];
        ++i;
        ++j;
    }
    method[i] = '\0';

    if(strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        unimplemented(client_sockfd);
        return nullptr;
    }

    if(strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    while(isspace(buf[j]) && (j < sizeof(buf)))
        ++j;

    while(isspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        ++i;
        ++j;
    }
    url[i] = '\0';

    // GET request url may have ? , with query parameters
    if(strcasecmp(method, "GET") == 0) {
        query_string = url;
        while((*query_string != '?') && (*query_string != '\0'))
            ++query_string;
        // If there is '?' which represent dynamic request, and open cgi.
        if(*query_string == '?') {
            cgi           = 1;
            *query_string = '\0';
            ++query_string;
        }
    }

    sprintf(path, "httpdocs%s", url);

    if(path[strlen(path) - 1] == '/') {
        strcat(path, "test.html");
    }
    else {
        if((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/test.html");
        if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP)
           || (st.st_mode & S_IXOTH))
            cgi = 1;
        if(!cgi)
            serve_file(client_sockfd, path);
        else
            execute_cgi(client_sockfd, path, method, query_string);
    }
    close(client_sockfd);
    return nullptr;
}

int main(void) {
    // set listening IP address
    std::string ip_addr;
    std::cout << "please set listen IP address!" << std::endl;
    std::cin >> ip_addr;
    // initialize some parameter
    int server_sockfd = -1, client_sockfd = -1;
    int port = 6379;  // default listening port is 6379
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    // TODO  using std::thread to rewrite it.
    pthread_t newthread;

    server_sockfd = startup(port);

    std::cout << "the number of http server socket is " << server_sockfd
              << std::endl;
    std::cout << "http server is running on port" << port << std::endl;

    while(1) {
        client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr,
                               &client_addr_len);
        if(client_sockfd == -1)
            error_die("accept");
        std::cout << "New connection ... ip:" << inet_ntoa(client_addr.sin_addr)
                  << ",port:" << ntohs(client_addr.sin_port) << std::endl;
        if(pthread_create(&newthread, nullptr, accept_request,
                          (void*)&client_sockfd)
           != 0) {
            error_die("thread create");
        }
    }
    return 0;
}
