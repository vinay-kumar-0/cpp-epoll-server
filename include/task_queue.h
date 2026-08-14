#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

struct Task {
    int client_fd; 
    bool is_udp;   
};

class TaskQueue {
private:
    std::queue<Task> queue;
    std::mutex mtx; 
    std::condition_variable cond_var; 
    bool shutdown; 

public:
    TaskQueue();
    ~TaskQueue();

    void push(const Task& task);
    std::optional<Task> pop(); 
    void stop();
};

#endif