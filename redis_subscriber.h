#ifndef REDIS_SUBSCRIBER_H
#define REDIS_SUBSCRIBER_H

#include <stdlib.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
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
#include <boost/tr1/functional.hpp>

class CRedisSubscriber
{
public:
    typedef std::tr1::function<void(const char *, const char *, int)> NotifyMessageFn; // 回调函数对象类型，当接收到消息后调用回调把消息发送出去

    CRedisSubscriber();
    CRedisSubscriber(std::string target_ip, int target_socket_port, int target_redis_port, int my_id);
    ~CRedisSubscriber();

    bool init(const NotifyMessageFn &fn); // 传入回调对象
    bool uninit();
    bool connect_redis();
    bool disconnect();

    // 可以多次调用，订阅多个频道
    bool subscribe(const std::string &channel_name);

    int epoll_fd_send_msg;
    std::mutex mutex;
    std::condition_variable not_empty;
    std::mutex size_mutex;
    int client_socket;
    std::list<RequestHeader> buffer_list;
    std::thread send_msg_thread;
    static uint64_t msg_cnt;
    void send_msg_loop();
    void enqueue(int seq, int site_id); 

private:
    // 下面三个回调函数供redis服务调用
    // 连接回调
    static void connect_callback(const redisAsyncContext *redis_context,
                                 int status);

    // 断开连接的回调
    static void disconnect_callback(const redisAsyncContext *redis_context,
                                    int status);

    // 执行命令回调
    static void command_callback(redisAsyncContext *redis_context,
                                 void *reply, void *privdata);

    // 事件分发线程函数
    static void *event_thread(void *data);
    void *event_proc();

    
    std::string target_ip = "127.0.0.1";
    int target_socket_port = 46000;
    int target_redis_port = 63790;
    uint32_t my_id = 0;

private:
    // libevent事件对象
    event_base *_event_base;
    // 事件线程ID
    pthread_t _event_thread;
    // 事件线程的信号量
    sem_t _event_sem;
    // hiredis异步对象
    redisAsyncContext *_redis_context;

    // 通知外层的回调函数对象
    NotifyMessageFn _notify_message_fn;
};

#endif