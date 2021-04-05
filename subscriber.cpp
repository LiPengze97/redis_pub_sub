#include "redis_subscriber.h"
#include <fstream>
#include <iostream>
#include <string.h>
#include <thread>
#include <memory>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "socket_utils.hpp"
using namespace std;

// class Client
// {
//     int local_id;
//     int epoll_fd_send_msg;
//     std::mutex mutex;
//     std::condition_variable not_empty;
//     std::mutex size_mutex;
//     int client_socket;
//     std::list<RequestHeader> buffer_list;
//     Client(string ip, int port, int local_id) : local_id(local_id)
//     {
//         epoll_fd_send_msg = epoll_create1(0);
//         if (epoll_fd_send_msg == -1)
//             throw std::runtime_error("failed to create epoll fd");
//         sockaddr_in serv_addr;
//         int fd = ::socket(AF_INET, SOCK_STREAM, 0);
//         if (fd < 0)
//             throw std::runtime_error("MessageSender failed to create socket.");
//         int flag = 1;
//         int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
//         if (ret == -1)
//         {
//             fprintf(stderr, "ERROR on setsockopt: %s\n", strerror(errno));
//             exit(-1);
//         }
//         memset(&serv_addr, 0, sizeof(serv_addr));
//         serv_addr.sin_family = AF_INET;
//         serv_addr.sin_port = htons(port);

//         inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);
//         if (connect(fd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
//         {
//             throw std::runtime_error("MessageSender failed to connect socket");
//         }
//         add_epoll(epoll_fd_send_msg, EPOLLOUT, fd);
//         client_socket = fd;
//     }
//     void send_msg_loop()
//     {
//         auto tid = pthread_self();
//         std::cout << "send_msg_loop start, tid = " << tid << std::endl;
//         struct epoll_event events[EPOLL_MAXEVENTS];
//         while (true)
//         {
//             // std::cout << "in send_msg_loop, thread_shutdown.load() is " << thread_shutdown.load() << std::endl;
//             std::unique_lock<std::mutex> lock(mutex);
//             not_empty.wait(lock, [this]() { return buffer_list.size() > 0; });
//             // has item on the queue to send
//             int n = epoll_wait(epoll_fd_send_msg, events, EPOLL_MAXEVENTS, -1);
//             // log_trace("epoll returned {} sockets ready for write", n);
//             for (int i = 0; i < n; i++)
//             {
//                 if (events[i].events & EPOLLOUT)
//                 {
//                     sock_write(events[i].data.fd, buffer_list.front());
//                     size_mutex.lock();
//                     buffer_list.pop_front();
//                     size_mutex.unlock();
//                 }
//             }
//         }
//     }
//     void enqueue(int seq, int site_id)
//     {
//         size_mutex.lock();
//         RequestHeader *header = new RequestHeader();
//         header->seq = seq;
//         header->site_id = site_id;
//         buffer_list.push_back(std::move(*header));
//         delete header;
//         size_mutex.unlock();
//         not_empty.notify_one();
//     }
// };
static void print_help(const char *cmd)
{
    std::cout << "Usage: " << cmd << " -i <target_ip>"
              << " -s <socket_port>"
              << " -r <redis_port>"
              << " -d <my_site_id>"
              << std::endl;
}
void recieve_message(const char *channel_name,
                     const char *message, int len)
{
    printf("Recieve message:\n    channel name: %s\n    message: %s\n",
           channel_name, message);
}

int main(int argc, char *argv[])
{
    string target_ip = "127.0.0.1";
    int target_socket_port = 46000;
    int target_redis_port = 63790;
    int my_id = 1;

    int opt;

    while ((opt = getopt(argc, argv, "i:s:r:d:")) != -1)
    {
        switch (opt)
        {
        case 'i':
            target_ip = optarg;
            break;
        case 's':
            target_socket_port = static_cast<std::size_t>(std::stoi(optarg));
            break;
        case 'r':
            target_redis_port = static_cast<std::size_t>(std::stoi(optarg));
            break;
        case 'd':
            my_id = static_cast<std::size_t>(std::stoi(optarg));
            break;
        default:
            print_help(argv[0]);
            return -1;
        }
    }

    CRedisSubscriber subscriber(target_ip, target_socket_port, target_redis_port, my_id);
    CRedisSubscriber::NotifyMessageFn fn =
        bind(recieve_message, std::tr1::placeholders::_1,
             std::tr1::placeholders::_2, std::tr1::placeholders::_3);
    bool ret = subscriber.init(fn);
    if (!ret)
    {
        printf("Init failed.\n");
        return 0;
    }

    ret = subscriber.connect_redis();
    if (!ret)
    {
        printf("Connect failed.\n");
        return 0;
    }

    subscriber.subscribe("test-channel");

    while (true)
    {
        sleep(1);
    }

    subscriber.disconnect();
    subscriber.uninit();

    return 0;
}