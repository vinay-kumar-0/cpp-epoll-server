#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <thread>
#include <unordered_set>
#include <mutex>
#include <string>
#include "task_queue.h"

class ThreadPool {
private:
    std::vector<std::thread> workers;
    TaskQueue& task_queue; 
    int epoll_fd;
    
    // --- NEW: State Management ---
    std::unordered_set<int> active_clients;
    std::mutex clients_mtx; // Protects the active_clients list

    void worker_loop(); 
    void broadcast(int sender_fd, const std::string& message);

public:
    ThreadPool(size_t num_threads, TaskQueue& queue, int epoll_fd);
    ~ThreadPool();

    // Let the Host (main.cpp) add new clients to the list
    void add_client(int client_fd);
    void remove_client(int client_fd);
};

#endif