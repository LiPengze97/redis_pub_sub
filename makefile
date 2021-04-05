EXE=server_main client_main
CC=g++
FLAG=-lhiredis -levent -lpthread -Wformat
OBJ=redis_publisher.o publisher.o redis_subscriber.o subscriber.o socket_utils.o

all:$(EXE)

$(EXE):$(OBJ)
	$(CC) -o publisher redis_publisher.o publisher.o socket_utils.o $(FLAG)
	$(CC) -o subscriber redis_subscriber.o subscriber.o socket_utils.o $(FLAG)

redis_publisher.o:redis_publisher.h
redis_subscriber.o:redis_subscriber.h
socket_utils.o:socket_utils.hpp

publisher.o:publisher.cpp
	$(CC) -c publisher.cpp

subscriber.o:subscriber.cpp
	$(CC) -c subscriber.cpp
clean:
	rm publisher subscriber *.o