CFLAGS = -Wall -g -Werror -Wno-error=unused-variable g++

COMMON_FILES = common.cpp

all: server subscriber

server: server.cpp common.cpp
	g++ -c common.cpp
	g++ -o server server.cpp common.cpp -Wall

subscriber: subscriber.cpp $(COMMON_FILES:.cpp=.o)
	g++ -o subscriber subscriber.cpp $(COMMON_FILES:.cpp=.o)

.PHONY: clean run_server run_client

run_server:
	./server
	
run_client:
	./subscriber

clean:
	rm -rf server subscriber common *.o *.dSYM

%.o: %.cpp
	$(CFLAGS) -c $<