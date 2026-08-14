CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -g

TARGET = server
SRCS = src/main.cpp src/task_queue.cpp src/thread_pool.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

clean:
	rm -f $(OBJS) $(TARGET)