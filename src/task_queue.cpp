#include "../include/task_queue.h"

TaskQueue::TaskQueue() : shutdown(false) {}
TaskQueue::~TaskQueue() {}

void TaskQueue::push(const Task& task) {
    std::unique_lock<std::mutex> lock(mtx);
    queue.push(task);
    lock.unlock(); // Unlock before notifying to save CPU cycles
    cond_var.notify_one();
}

std::optional<Task> TaskQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);
    
    // Go to sleep until a task arrives OR we shut down
    cond_var.wait(lock, [this] { 
        return !queue.empty() || shutdown; 
    });

    if (shutdown && queue.empty()) {
        return std::nullopt; 
    }

    Task task = queue.front();
    queue.pop();
    return task;
}

void TaskQueue::stop() {
    std::unique_lock<std::mutex> lock(mtx);
    shutdown = true;
    lock.unlock();
    cond_var.notify_all(); // Wake everyone up to exit cleanly
}