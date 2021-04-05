#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <sys/epoll.h>
#include "redis_subscriber.h"
/** Type alias for IP addresses, currently stored as strings. */
using ip_addr_t = std::string;
/** Type alias for Site IDs. */
using site_id_t = uint32_t;
inline uint64_t get_time_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
uint64_t CRedisSubscriber::msg_cnt = 0;
CRedisSubscriber::CRedisSubscriber() : _event_base(0), _event_thread(0),
                                       _redis_context(0)
{
}

CRedisSubscriber::CRedisSubscriber(std::string target_ip, int target_socket_port, int target_redis_port, int my_id) : _event_base(0), _event_thread(0),
                                                                                                                      _redis_context(0), target_ip(target_ip), target_socket_port(target_socket_port), my_id(my_id), target_redis_port(target_redis_port)
{
    epoll_fd_send_msg = epoll_create1(0);
    if (epoll_fd_send_msg == -1)
        throw std::runtime_error("failed to create epoll fd");
    sockaddr_in serv_addr;
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error("MessageSender failed to create socket.");
    int flag = 1;
    int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));
    if (ret == -1)
    {
        fprintf(stderr, "ERROR on setsockopt: %s\n", strerror(errno));
        exit(-1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(target_socket_port);

    inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr);
    if (connect(fd, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        throw std::runtime_error("MessageSender failed to connect socket");
    }
    add_epoll(epoll_fd_send_msg, EPOLLOUT, fd);
    client_socket = fd;

    send_msg_thread = std::thread(&CRedisSubscriber::send_msg_loop, this);
    send_msg_thread.detach();
}

CRedisSubscriber::~CRedisSubscriber()
{
}

bool CRedisSubscriber::init(const NotifyMessageFn &fn)
{
    // initialize the event
    _notify_message_fn = fn;
    _event_base = event_base_new(); // 创建libevent对象
    if (NULL == _event_base)
    {
        printf(": Create redis event failed.\n");
        return false;
    }

    memset(&_event_sem, 0, sizeof(_event_sem));
    int ret = sem_init(&_event_sem, 0, 0);
    if (ret != 0)
    {
        printf(": Init sem failed.\n");
        return false;
    }

    return true;
}

bool CRedisSubscriber::uninit()
{
    _event_base = NULL;

    sem_destroy(&_event_sem);
    return true;
}

bool CRedisSubscriber::connect_redis()
{
    // connect redis
    _redis_context = redisAsyncConnect(target_ip.c_str(), target_redis_port); // 异步连接到redis服务器上，使用默认端口
    if (NULL == _redis_context)
    {
        printf(": Connect redis failed.\n");
        return false;
    }

    if (_redis_context->err)
    {
        printf(": Connect redis error: %d, %s\n",
               _redis_context->err, _redis_context->errstr); // 输出错误信息
        return false;
    }

    // attach the event
    redisLibeventAttach(_redis_context, _event_base); // 将事件绑定到redis context上，使设置给redis的回调跟事件关联

    // 创建事件处理线程
    int ret = pthread_create(&_event_thread, 0, &CRedisSubscriber::event_thread, this);
    if (ret != 0)
    {
        printf(": create event thread failed.\n");
        disconnect();
        return false;
    }

    // 设置连接回调，当异步调用连接后，服务器处理连接请求结束后调用，通知调用者连接的状态
    redisAsyncSetConnectCallback(_redis_context,
                                 &CRedisSubscriber::connect_callback);

    // 设置断开连接回调，当服务器断开连接后，通知调用者连接断开，调用者可以利用这个函数实现重连
    redisAsyncSetDisconnectCallback(_redis_context,
                                    &CRedisSubscriber::disconnect_callback);

    // 启动事件线程
    sem_post(&_event_sem);
    return true;
}

bool CRedisSubscriber::disconnect()
{
    if (_redis_context)
    {
        redisAsyncDisconnect(_redis_context);
        redisAsyncFree(_redis_context);
        _redis_context = NULL;
    }

    return true;
}

bool CRedisSubscriber::subscribe(const std::string &channel_name)
{
    int ret = redisAsyncCommand(_redis_context,
                                &CRedisSubscriber::command_callback, this, "SUBSCRIBE %s",
                                channel_name.c_str());
    if (REDIS_ERR == ret)
    {
        printf("Subscribe command failed: %d\n", ret);
        return false;
    }

    printf(": Subscribe success: %s\n", channel_name.c_str());
    return true;
}

void CRedisSubscriber::connect_callback(const redisAsyncContext *redis_context,
                                        int status)
{
    if (status != REDIS_OK)
    {
        printf(": Error: %s\n", redis_context->errstr);
    }
    else
    {
        printf(": Redis connected!");
    }
}

void CRedisSubscriber::disconnect_callback(
    const redisAsyncContext *redis_context, int status)
{
    if (status != REDIS_OK)
    {
        // 这里异常退出，可以尝试重连
        printf(": Error: %s\n", redis_context->errstr);
    }
}

// 消息接收回调函数
void CRedisSubscriber::command_callback(redisAsyncContext *redis_context,
                                        void *reply, void *privdata)
{
    if (NULL == reply || NULL == privdata)
    {
        return;
    }

    // 静态函数中，要使用类的成员变量，把当前的this指针传进来，用this指针间接访问
    CRedisSubscriber *self_this = reinterpret_cast<CRedisSubscriber *>(privdata);
    redisReply *redis_reply = reinterpret_cast<redisReply *>(reply);

    // 订阅接收到的消息是一个带三元素的数组
    if (redis_reply->type == REDIS_REPLY_ARRAY &&
        redis_reply->elements == 3)
    {
        printf("Recieve message:%s:%ld:%s:%ld:%s:%ld\n",
               redis_reply->element[0]->str, redis_reply->element[0]->len,
               redis_reply->element[1]->str, redis_reply->element[1]->len,
               redis_reply->element[2]->str, redis_reply->element[2]->len);

        // 调用函数对象把消息通知给外层
        self_this->_notify_message_fn(redis_reply->element[1]->str,
                                      redis_reply->element[2]->str, redis_reply->element[2]->len);
        if (redis_reply->element[2]->len != 0)
        {
            //do send the ack back to source
            self_this->size_mutex.lock();
            RequestHeader *header = new RequestHeader();
            header->seq = msg_cnt++;
            printf("msg_cnt is : %lu\n", msg_cnt);
            header->site_id = self_this->my_id;
            self_this->buffer_list.push_back(std::move(*header));
            delete header;
            self_this->size_mutex.unlock();
            self_this->not_empty.notify_one();
        }
    }
}

void *CRedisSubscriber::event_thread(void *data)
{
    if (NULL == data)
    {
        printf(": Error!\n");
        assert(false);
        return NULL;
    }

    CRedisSubscriber *self_this = reinterpret_cast<CRedisSubscriber *>(data);
    return self_this->event_proc();
}

void *CRedisSubscriber::event_proc()
{
    sem_wait(&_event_sem);

    // 开启事件分发，event_base_dispatch会阻塞
    event_base_dispatch(_event_base);

    return NULL;
}

void CRedisSubscriber::send_msg_loop()
{
    auto tid = pthread_self();
    std::cout << "send_msg_loop start, tid = " << tid << std::endl;
    struct epoll_event events[EPOLL_MAXEVENTS];
    while (true)
    {
        // std::cout << "in send_msg_loop, thread_shutdown.load() is " << thread_shutdown.load() << std::endl;
        std::unique_lock<std::mutex> lock(mutex);
        not_empty.wait(lock, [this]() { return buffer_list.size() > 0; });
        // has item on the queue to send
        int n = epoll_wait(epoll_fd_send_msg, events, EPOLL_MAXEVENTS, -1);
        // log_trace("epoll returned {} sockets ready for write", n);
        for (int i = 0; i < n; i++)
        {
            if (events[i].events & EPOLLOUT)
            {
                sock_write(events[i].data.fd, buffer_list.front());
                size_mutex.lock();
                buffer_list.pop_front();
                size_mutex.unlock();
            }
        }
    }
}

void CRedisSubscriber::enqueue(int seq, int site_id)
{
}