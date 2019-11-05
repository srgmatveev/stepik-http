#include <cstdint>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <cerrno>
#include <sys/epoll.h>
#include <zconf.h>
#include <cstring>
#include <pthread.h>
#include "get_opt.h"

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10
struct fds {
    int epollfd;
    int sockfd;
};

int set_nonblock(int fd) {
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, (unsigned) flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIONBIO, &flags);
#endif

}

void AddFd(int epollfd, int fd, bool oneshot) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (oneshot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    set_nonblock(fd);
}

void reset_oneshot(int &epfd, int &fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event);
}

void *worker(void *arg) {
    int sockfd = ((struct fds *) arg)->sockfd;
    int epollfd = ((struct fds *) arg)->epollfd;
    printf("start new thread to receive data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, 0, BUFFER_SIZE);

    while (1) {
        int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
        if (ret == 0) {
            close(sockfd);
            printf("foreigner closed the connection\n");
            break;
        } else if (ret < 0) {
            if (errno = EAGAIN) {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        } else {
            printf("get content: %s\n", buf);
            //Hibernate for 5 seconds to simulate data processing
            printf("worker working...\n");
            sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
    pthread_exit(0);
}


int main(const int argc, const char **argv) {
    int masterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (masterSocket < 0)
        perror("fail to create socket!\n"), exit(errno);
    char *serv_dir = nullptr;
    struct sockaddr_in sock_addr;
    bzero(&sock_addr, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    get_command_line(argc, (char **) (argv), sock_addr, serv_dir);
    int opt = 1;
    if (setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        exit(errno);
    }
    if (bind(masterSocket, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) < 0) {
        perror("fail to bind socket");
        exit(errno);
    }

    set_nonblock(masterSocket);
    listen(masterSocket, SOMAXCONN);

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create1(0);
    if (epollfd == -1)
        perror("fail to create epoll\n"), exit(errno);

    AddFd(epollfd, masterSocket, false);

    for(;;){
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);  //Permanent Wait
        if(ret < 0)
        {
            printf("epoll wait failure!\n");
            break;
        }
        int i;
        for(i = 0; i < ret; i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == masterSocket)
            {
                struct sockaddr_in slave_address;
                socklen_t slave_addrlength = sizeof(slave_address);
                int slaveSocket = accept(masterSocket, (struct sockaddr*)&slave_address, &slave_addrlength);
                AddFd(epollfd, slaveSocket, true);
            }
            else if(events[i].events & EPOLLIN)
            {
                pthread_t thread;
                struct fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = events[i].data.fd;
                /*Start a new worker thread to serve sockfd*/
                pthread_create(&thread, NULL, worker, &fds_for_new_worker);

            }
            else
            {
                printf("something unexpected happened!\n");
            }
        }

    }
    close(masterSocket);
    close(epollfd);
    return 0;
}