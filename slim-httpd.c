#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <err.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/resource.h>

#include "zyzi_http.h"
#include "zyzi_log.h"
#include "zyzi_event.h"
#include "zyzi_malloc.h"

#include "slim-httpd.h"

const char SERV_NAME[] = "SimpleHttp (version 0.0.1)";
const char exec_name[] = "slim-httpd";

char *sock_ntop(const struct sockaddr *addr , socklen_t len)
{
    char		portstr[8];
    static char str[128];

    switch (addr->sa_family) {
    case AF_INET: {
        struct sockaddr_in *sin =  (struct sockaddr_in *) addr;

        if (inet_ntop(AF_INET , &sin->sin_addr , str , sizeof(str)) == NULL) {
            return NULL;
        }

        if (sin->sin_port != 0)  {
            snprintf(portstr , sizeof(portstr) , ":%d" , ntohs(sin->sin_port));
            strcat(str , portstr);
        }

        return str;
    }
    /* end AF_INET */
    }

    return NULL;
}

int daemon_init(const char *pname)
{
    int i;
    struct rlimit rl;
    
    if (getrlimit(RLIMIT_NOFILE , &rl) == -1) {
        err(EXIT_FAILURE , "getrlimit() failed");
    }

    switch (fork()) {
    case -1:
        return -1;
    case  0:
        break;
    default:
        _exit(0); /* parent terminates */ 
    }

    /* child 1 continues... */
    if (setsid() == -1) { /* become session leader */
        return -1;
    }

    signal(SIGHUP , SIG_IGN);

    switch (fork()) {
    case -1:
        return -1;
    case  0:
        break;
    default:
        _exit(0); /* child 1 terminates */   
    }
    
    /* child 2 continues... */
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }

    for (i = 0 ; i < rl.rlim_max ; i++) {
        close(i);
    }

    open("/dev/null" , O_RDONLY); /* redirect stdin */
    open("/dev/null" , O_RDWR);   /* redirect stdout */
    open("/dev/null" , O_RDWR);   /* redirect stderr */

    daemon_proc = 1;
    zyzi_create_log("shttpd.log");
    
    return 0;
}
struct {
    int   daemo_mode;
    int   server_listen_port;
    char *server_listen_addr;
    char *server_listen_path;
} server_ctx = {
    .daemo_mode = 0,
    .server_listen_port = 80,
    .server_listen_addr = "0.0.0.0",
    .server_listen_path = NULL
};

void parse_commandline(int argc , char *argv[])
{
    if (argc == 1) return;

    int i;
    for (i = 1 ; i < argc ; i++) {
        if (strcmp(argv[i], "--port") == 0) {
            if (++i >= argc) {
                errx(EXIT_FAILURE, "missing number after --port");
            }
            server_ctx.server_listen_port = atoi(argv[i]);
        } else if (strcmp(argv[i], "--addr") == 0) {
            if (++i >= argc) {
                errx(EXIT_FAILURE, "missing ip after --port");
            }
            server_ctx.server_listen_addr = argv[i];
        } else if (strcmp(argv[i], "--path") == 0) {
            if (++i >= argc) {
                errx(EXIT_FAILURE, "missing path after --port");
            }
            server_ctx.server_listen_path = argv[i];
        } else if (strcmp(argv[i], "--d") == 0){
            server_ctx.daemo_mode = 1;
        } else {
            errx(EXIT_FAILURE , "unknown argument '%s'", argv[i]);
        }
    }
}

int create_tcp_listen(void)
{
    int sock , opt_on;
    struct sockaddr_in addr;
    bzero(&addr , sizeof(addr));

    addr.sin_family = AF_INET;

    if (inet_pton(AF_INET , server_ctx.server_listen_addr , &addr.sin_addr) == -1) {
        zyzi_log(ZYZI_LOG_ERRO, "invalid ip address '%s'" , server_ctx.server_listen_addr);
    }

    if (server_ctx.server_listen_port <= 0 && server_ctx.server_listen_port > 65535) {
        zyzi_log(ZYZI_LOG_ERRO, "invalid port: '%d'" , server_ctx.server_listen_port);
    }

    addr.sin_port = htons(server_ctx.server_listen_port);

    if ((sock = socket(AF_INET , SOCK_STREAM , 0)) == -1) {
        zyzi_logx(ZYZI_LOG_ERRO , "socket() failed");
    }

    opt_on = 1;
    setsockopt(sock , SOL_SOCKET , SO_REUSEADDR , &opt_on , sizeof(opt_on));

    if (bind(sock , (struct sockaddr *) &addr , sizeof(addr)) == -1) {
        zyzi_logx(ZYZI_LOG_ERRO , "bind() failed");
    }

    if (listen(sock , SOMAXCONN) == -1) {
        zyzi_logx(ZYZI_LOG_ERRO , "listen() failed");
    }

    zyzi_log(ZYZI_LOG_INFO , "listen %s:%d" , server_ctx.server_listen_addr , server_ctx.server_listen_port);

    return sock;
}

void httpRequestHandler(zyziEventLoop *eventLoop , int fd , int mask , void *data)
{
    (void) mask;
    (void) data;

    char buff[1 << 15];
    size_t n;

    n = read(fd , buff , 1 << 15);

	if (n == 0) {
        close(fd);

        zyziDeleteEventFile(eventLoop , fd , ZYZI_READABLE);

        return;
    } else if (n == -1) {
        if (errno != EWOULDBLOCK) {
            zyzi_logx(ZYZI_ERR , "read() failed");
        }

        return;
    }

    zyzi_http_head *head = zyziProcessHttpRequest(buff , n , fd);

    if (head) {
        zyziHttpSend(head);
        zyziHttpHeadDestroy(head);
    }

   close(fd);

    zyziDeleteEventFile(eventLoop , fd , ZYZI_READABLE);
}

void tcpOnAccept(zyziEventLoop *eventLoop , int fd , int mask , void *data)
{
    (void) mask;
    (void) data;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    int clnt = accept(fd , (struct sockaddr *) &addr , &len);
    if (clnt == -1) {
        if (errno != EWOULDBLOCK) {
            zyzi_logx(ZYZI_LOG_ERRO , "accept() failed");
        }
    } else {
		zyzi_log(ZYZI_LOG_INFO , "connect by %s" , sock_ntop((struct sockaddr *) &addr , len));

        zyziCreateEventFile(eventLoop , clnt , ZYZI_READABLE , httpRequestHandler , NULL);
    }
}

void sigHandler(int sig_no)
{
    (void) sig_no;
    
    zyzi_close_log();
    
    exit(0);
}

int main(int argc , char *argv[])
{
    parse_commandline(argc , argv);

    signal(SIGPIPE , SIG_IGN);
    signal(SIGTERM , sigHandler);
    
    if (server_ctx.daemo_mode) {
        if (daemon_init(exec_name)) {
            errx(EXIT_FAILURE , "daemon_init() failed");
        }
    } else {
        zyzi_create_log("shttpd.log");
    }
    
    if (server_ctx.server_listen_path) {
        if (chdir(server_ctx.server_listen_path) == -1) {
            zyzi_logx(ZYZI_LOG_ERRO , "chdir() failed");
            exit(EXIT_FAILURE);
        }
    }
    
    int lsock = create_tcp_listen();

    zyziEventLoop *eventLoop = zyziCreateEventLoop(1024);

    if (!eventLoop) {
        zyzi_log(ZYZI_LOG_ERRO , "zyziCreateEventLoop() failed");
    } else {
        zyziCreateEventFile(eventLoop , lsock , ZYZI_READABLE , tcpOnAccept , NULL);

        zyziMain(eventLoop);
    }

    return 0;
}
