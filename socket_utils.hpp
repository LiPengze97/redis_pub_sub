#pragma once
#include <iostream>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#define EPOLL_MAXEVENTS 64

int add_epoll(int epoll_fd, int events, int fd);

bool sock_read(int sock, char* buffer, size_t size);

template <typename T>
bool sock_read(int sock, T& obj) {
    return sock_read(sock, reinterpret_cast<char*>(&obj), sizeof(obj));
};

bool sock_write(int sock, const char* buffer, size_t size);

template <typename T>
bool sock_write(int sock, const T& obj) {
    return sock_write(sock, reinterpret_cast<const char*>(&obj), sizeof(obj));
};

struct RequestHeader {
    uint64_t seq;
    uint32_t site_id;
};

struct Response {
    size_t payload_size;
    uint64_t version;
    uint64_t seq; //this is for read request
    uint32_t site_id;
};