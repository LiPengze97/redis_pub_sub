#include "redis_publisher.h"
#include <fstream>
#include <iostream>
#include <string.h>
#include <thread>
#include <memory>
#include <vector>
#include <cstring>
#include <netinet/tcp.h>
#include "socket_utils.hpp"
using namespace std;

/** Type alias for IP addresses, currently stored as strings. */
using ip_addr_t = std::string;
/** Type alias for Site IDs. */
using site_id_t = uint32_t;

class Server
{
private:
    size_t num_senders;

    std::vector<std::thread> worker_threads;

    int server_socket;

public:
    Server(int num_senders, unsigned short local_port) : num_senders(num_senders)
    {
        std::cout << "1 " << std::endl;
        sockaddr_in serv_addr;
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            throw std::runtime_error("RemoteMessageService failed to create socket.");

        int reuse_addr = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse_addr,
                       sizeof(reuse_addr)) < 0)
        {
            fprintf(stderr, "ERROR on setsockopt: %s\n", strerror(errno));
        }

        int flag = 1;
        int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

        if (ret == -1)
        {
            fprintf(stderr, "ERROR on setsockopt: %s\n", strerror(errno));
            exit(-1);
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(local_port);
        if (bind(fd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            fprintf(stderr, "ERROR on binding to socket: %s\n", strerror(errno));
            throw std::runtime_error("RemoteMessageService failed to bind socket.");
        }
        listen(fd, 5);
        server_socket = fd;
        std::cout << "RemoteMessageService listening on " << local_port << std::endl;
    };

    void establish_connections()
    {
        auto num_fd = (num_senders << 1);
        printf("%d num_df\n", (int)num_fd);
        while (worker_threads.size() < num_fd)
        {
            struct sockaddr_storage client_addr_info;
            socklen_t len = sizeof client_addr_info;

            int connected_sock_fd = ::accept(server_socket, (struct sockaddr *)&client_addr_info, &len);
            worker_threads.emplace_back(std::thread(&Server::epoll_worker, this, connected_sock_fd));
        }
        for(int i = 0; i < worker_threads.size(); i++){
            worker_threads[i].join();
        }
    }

    void epoll_worker(int connected_sock_fd)
    {
        RequestHeader header;
        bool success;
        std::cout << "epoll_worker start\n";

        int epoll_fd_recv_msg = epoll_create1(0);
        if (epoll_fd_recv_msg == -1)
            throw std::runtime_error("failed to create epoll fd");
        add_epoll(epoll_fd_recv_msg, EPOLLIN, connected_sock_fd);

        std::cout << "The connected_sock_fd is " << connected_sock_fd << std::endl;

        struct epoll_event events[EPOLL_MAXEVENTS];
        while (true)
        {
            int n = epoll_wait(epoll_fd_recv_msg, events, EPOLL_MAXEVENTS, -1);
            for (int i = 0; i < n; i++)
            {
                if (events[i].events & EPOLLIN)
                {
                    success = sock_read(connected_sock_fd, header);
                    if (!success)
                    {
                        std::cout << "Failed to read request header, "
                                  << "receive " << n << " messages from sender.\n";
                        throw std::runtime_error("Failed to read request header");
                    }
                    //处理header的seq和site id就行
                    printf("site_id %d, seq %lu\n", header.site_id, header.seq);
                }
            }
        }
    }

    bool is_server_ready();
};

string chatting_split(string str)
{
    string res = "";
    if (str == "")
        return res;
    size_t pos = str.find(" ", 0);
    if (pos != str.npos)
    {
        string tmp = str.substr(pos + 1, str.size());
        pos = tmp.find(" ", 0);
        if (pos != tmp.npos)
        {
            res = tmp.substr(pos + 1, str.size());
        }
    }
    return res;
}

static void print_help(const char *cmd)
{
    std::cout << "Usage: " << cmd << " -p <port>"
              << " [-n number_of_senders]"
              << std::endl;
}

int main(int argc, char *argv[])
{
    int opt;
    int port = 46000;
    int num_of_senders = 1;

    while ((opt = getopt(argc, argv, "p:n:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            port = static_cast<std::size_t>(std::stoi(optarg));
            break;
        case 'n':
            num_of_senders = static_cast<std::size_t>(std::stoi(optarg));
            break;
        default:
            print_help(argv[0]);
            return -1;
        }
    }

    Server server(num_of_senders, port);
    printf("hi there.\n");
    std::thread rms_establish_thread(&Server::establish_connections, &server);
    rms_establish_thread.detach();

    CRedisPublisher publisher;

    bool ret = publisher.init();
    if (!ret)
    {
        printf("Init failed.\n");
        return 0;
    }

    ret = publisher.connect();
    if (!ret)
    {
        printf("connect failed.");
        return 0;
    }
    printf("Press enter to start publish message!\n");
    getchar();
    ifstream infile;
    infile.open("/users/pzl97/angularjs.txt");

    string s;
    int a = 0;
    while (getline(infile, s))
    {
        // cout << chatting_split(s) << endl;
        publisher.publish("test-channel", s);
        sleep(1);
        if (a++ > 50)
        {
            break;
        }
    }
    infile.close();
    publisher.disconnect();
    publisher.uninit();
    return 0;
}