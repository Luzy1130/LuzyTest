#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>        /* basic socket definitions */
#include <netinet/in.h>        /* sockaddr_in and other internet defns */
#include <arpa/inet.h>         /* inet(3) functions */
#include <fcntl.h>             /* setnonblocing */
#include <sys/epoll.h>         /* epoll functions */
#include <sys/resource.h>      /* setrlimit */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "type.h"

#define MAXEPOLLSIZE 10000
#define MAXLINE 10240


s32 handleClient(s32 connfd)
{
    s32 nread;
    char buf[MAXLINE] = {0};
    
    nread = read(connfd, buf, MAXLINE);
    if (nread == 0) {
        printf("client close the connection\n");
        close(connfd);
        return -1;
    }
    if(nread < 0) {
        perror("read error");
        close(connfd);
        return -1
    }

    write(connfd, buf, nread);
    return 0;
}


int main(int argc, char** argv)
{
    u32 serverPort = 1212;
    s32 listenq = 1024;

    s32 listenfd;
    s32 connfd;
    s32 kdpfd;
    s32 nfds;
    s32 n;
    s32 nread;
    s32 curfds;
    s32 acceptCount = 0; 
    struct sockaddr_in serverAddr, cliAddr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    struct epoll_event ev;
    struct epoll_event events[MAXEPOLLSIZE];
    struct rlimit rt;
    char buf[MAXLINE];

    /* Set max files can bee opened by this process. */
    rt.rlim_max = rt.rlim_cur = MAXEPOLLSIZE;
    if (setrlimit(RLIMIT_NOFILE, &rt) == -1) {
        perror("setrlimit error");
        return -1;
    }

    memset(buf, MAXLINE, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
//    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(serverPort);    
 
    /* 1. Create socket */   
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        perror("create socket fail");
        return -1;
    }

    s32 opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
     
    if (setnonblocking(listenfd) < 0) {
        perror("set non blocking fail");
    }
    
    /* 2. Bind socket on server port */
    if (bind(listenfd, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr)) == -1) {
        perror("bind error");
        return -1;
    }
    
    /* 3. Begin to listen this fd */
    if (listen(listenfd, listenq) == -1) {
        perror("listen error");
        return -1;
    }

    /* Create epoll fd, and add listenfd into epoll set. */
    kdpfd = epoll_create(MAXEPOLLSIZE);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listenfd;
    if (epoll_ctl(kdpfd, EPOLL_CTL_ADD, listenfd, &ev) < 0) {
        fprintf(strerr, "epoll set insertion error: fd=%d\n", listenfd);
        close(kdpfd);
        return -1;
    }    

    curfds = 1;
    printf("epool server starts, port %d, maxConnection is %d, backlogi is %d\n", serverPort, MAXEPOLLSIZE, listenq);

    while(1) {
        nfds = epoll_wait(kdpfd, events, curfds, -1);
        if (nfds == -1) {
            perror("epoll wait");
            continue;
        }
        
        /* handle the events */
        for (s32 i = 0; i < nfds; i++) {
            /* Handle the connect event */
            if (events[i].data.fd == listenfd) {
                connfd = accept(listenfd, (struct sockaddr*)&cliAddr, &socklen);
                if (connfd < 0) {
                    perror("accept error");
                    continue;
                }
        
                sprintf(buf, "accept from %s:%d\n", inet_ntoa(cliAddr.sin_addr), cliAddr.sin_port);
                printf("%d:%s", ++acceptCount, buf);
                
                /* Reach the limit of fds, reject it. */
                if (curfds >= MAXEPOLLSIZE) {
                    fprintf(stderr, "number of fds outofrange, max: %d\n", MAXEPOLLSIZE);
                    close(connfd);
                    continue;
                }

                /* set connfd nonblocking */
                if (setnonblocking(connfd) < 0) {
                    perror("setnoblocking error");
                }

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = connfd;
                if(epoll_ctl(kdpfd, EPOLL_CTL_ADD, connfd, &ev) < 0) {
                    fprintf(stderr, "add socket %d to epoll failed: %s\n", connfd, strerror(errno));
                    continue;
                }
                curfds++;
                continus;
            }
    
            /* Handle the data sended fron client */
            if(handleClient(events[i].data.fd < 0)) {
                epoll_ctl(kdpfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                curfd--;
            }
        }
    }

    close(kdpfd);
    close(listenfd);
    return 0;
}
