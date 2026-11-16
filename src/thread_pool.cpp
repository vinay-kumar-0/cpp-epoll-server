#include "../include/thread_pool.h"
#include <iostream>
#include <unistd.h> 
#include <cstring>  
 
#include <sys/socket.h> // NEW: For recvfrom and sendto
#include  <netinet/in.h>// NEW: For sockaddr_in
#include <sys/epoll.h>


ThreadPool::ThreadPool(size_t num_threads, TaskQueue& queue, int epoll_fd)
    : task_queue(queue), epoll_fd(epoll_fd) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(&ThreadPool::worker_loop, this);
    }
}

ThreadPool::~ThreadPool() {
    task_queue.stop(); 
    for (auto& worker : workers) {
        if (worker.joinable()) worker.join(); 
    }
}

void ThreadPool::add_client(int client_fd) {
    std::unique_lock lock(clients_mtx);
    active_clients.insert(client_fd);
}

void ThreadPool::remove_client(int client_fd) {
    std::unique_lock lock(clients_mtx);
    active_clients.erase(client_fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd); 
}

void ThreadPool::broadcast(int sender_fd, const std::string& message) {
    std::unique_lock lock(clients_mtx);
    for (int client_fd : active_clients) {
        if (client_fd != sender_fd) {
            write(client_fd, message.c_str(), message.length());
        }
    }
}

void ThreadPool::worker_loop() {
    while (true) {
        std::optional task = task_queue.pop();
        if (!task.has_value()) break; 

        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));

        if (task->is_udp) {
            // --- NEW: UDP LOGIC (Connectionless) ---
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);
            
            // Grab the packet AND the sender's IP/Port simultaneously
            ssize_t bytes = recvfrom(task->client_fd, buffer, sizeof(buffer) - 1, 0, 
                                     (struct sockaddr*)&sender_addr, &sender_len);
            
            if (bytes > 0) {
                std::cout << "Worker [" << std::this_thread::get_id() 
                          << "] received UDP packet: " << buffer;
                
                // Send a fast acknowledgment directly back to the sender
                std::string ack = "UDP Packet ACK from Server\n";
                sendto(task->client_fd, ack.c_str(), ack.length(), 0, 
                       (struct sockaddr*)&sender_addr, sender_len);
            }
        } else {
            // --- EXISTING: TCP LOGIC (Connection-Oriented) ---
            ssize_t bytes_read = read(task->client_fd, buffer, sizeof(buffer) - 1);

            if (bytes_read > 0) {
                if (bytes_read >= 4 && strncmp(buffer, "GET ", 4) == 0) {
                    const char response_body[] =
                        "Vinay's C++ Epoll Server is active! Connect via a TCP client to chat.";
                    const std::string response =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: " + std::to_string(strlen(response_body)) + "\r\n"
                        "Connection: close\r\n"
                        "\r\n" + response_body;

                    send(task->client_fd, response.c_str(), response.length(), 0);
                    remove_client(task->client_fd);
                    continue;
                }

                std::string msg = "User " + std::to_string(task->client_fd) + ": " + std::string(buffer);
                std::cout << "Broadcasting: " << msg;
                broadcast(task->client_fd, msg);
            } 
            else if (bytes_read == 0) {
                std::cout << "User " << task->client_fd << " left the chat.\n";
                remove_client(task->client_fd);
            }
        }
    }
}