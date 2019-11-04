#include <cstdint>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <cerrno>
#include <sys/epoll.h>
#include <zconf.h>
#include <cstring>
#include "get_opt.h"

#define MAX_EVENTS 32
struct sockaddr_in sockAddr;
char *serv_dir = nullptr;
int server_fd;

int set_nonblock(int fd) {
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, (unsigned)flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIONBIO, &flags);
#endif

}

int main(const int argc, const char **argv) {
    int masterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockAddr.sin_family = AF_INET;
    get_command_line(argc, (char **) (argv), sockAddr, serv_dir);
    int opt = 1;
    if (setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        exit(errno);
    }
    if (bind(masterSocket, (struct sockaddr *) &sockAddr, sizeof(sockAddr)) < 0) {
        perror("Bind");
        exit(errno);
    }

    set_nonblock(masterSocket);
    listen(masterSocket, SOMAXCONN);
    int efd = epoll_create1(0);

    struct epoll_event event{0};
    event.data.fd = masterSocket;
    event.events = EPOLLIN;
    epoll_ctl(efd, EPOLL_CTL_ADD, masterSocket, &event);

    for (;;) {
        struct epoll_event events[MAX_EVENTS];
        int N = epoll_wait(efd, events, MAX_EVENTS, -1);
        for (size_t i = 0; i < N; ++i) {
            if (events[i].data.fd == masterSocket) {
                int slaveSocket = accept(masterSocket, nullptr, nullptr);
                set_nonblock(slaveSocket);
                struct epoll_event event_slave{0};
                event_slave.data.fd = slaveSocket;
                event_slave.events = EPOLLIN;
                epoll_ctl(efd, EPOLL_CTL_ADD, slaveSocket, &event_slave);
            } else {
                static char BUFFER[1024];
                memset(BUFFER,'\0',1024);
                const char *ex = "exit\r\n";
                int recvResult = recv(events[i].data.fd,
                                      BUFFER, 1024, MSG_NOSIGNAL);
                if (!recvResult && errno != EAGAIN) {
                    shutdown(events[i].data.fd, SHUT_RDWR);
                    close(events[i].data.fd);
                } else if (recvResult > 0) {
                    send(events[i].data.fd, BUFFER, recvResult, MSG_NOSIGNAL);
                    if (strcmp(ex, BUFFER) == 0)
                        return 0;

                }

            }
        }
    }
}