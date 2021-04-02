# redis_pub_sub
Actually, this is not my code. I modify them from [blogs](http://www.voidcn.com/article/p-abyrqomf-bcx.html) online.
# Prerequisite
Some software packages.
```
sudo apt-get update
sudo apt-get install -y redis-server
sudo apt-get install -y libevent-dev
sudo apt-get install -y libboost-dev
```
And hiredis, a C++ redis interface.
```
git clone https://github.com/redis/hiredis
cd hiredis
mkdir build 
cd build
cmake ..
make -j
sudo make install
cd ../..
```
# Install & Usage
To install it, just:
```
make
```
To start the subscriber:
```
./subscriber
```
To start the publisher:
```
./publisher
```

# Set the right ip and port for redis
Notice that, the redis does not support being accessed from remote. To make it can support remote access, you need to change the redis conf file in the `/etc/redis/redis.conf`

then set the `bind 127.0.0.1` to `0.0.0.0`

Moreover, if you change the default port, you also need to change them in the code. The related files are `redis_publisher.cpp`and`redis_subscriber.cpp`