CXX = g++
CXXFLAGS = -O3 -march=native -mtune=native -std=c++17 -pthread \
           -Iinclude -Isrc

# ===== SERVER FILES =====
SERVER_SRCS = \
    src/server/server.cpp \
    src/flexql/flexql.cpp \
    src/parser/parser.cpp \
    src/query/executor.cpp \
    src/cache/lru_cache.cpp

SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)

# ===== CLIENT FILE =====
CLIENT_SRC = src/client/client.cpp

# ===== TARGETS =====
all: server client

server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o server $(SERVER_OBJS)

client:
	$(CXX) $(CXXFLAGS) -o client $(CLIENT_SRC)

# Compile rule
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run:
	./server

run_client:
	./client

clean:
	rm -f $(SERVER_OBJS) server client