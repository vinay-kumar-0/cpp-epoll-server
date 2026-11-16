#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include "../include/task_queue.h"
#include "../include/thread_pool.h"

#define MAX_EVENTS 10
#define TCP_PORT 8080
#define UDP_PORT 8081

void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    TaskQueue queue;

    // --- 1. SETUP TCP SOCKET (The Front Door for Text) ---
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_fd == -1) return 1;

    struct sockaddr_in tcp_addr;
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = INADDR_ANY;
    tcp_addr.sin_port = htons(TCP_PORT);
    
    bind(tcp_fd, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr));
    listen(tcp_fd, SOMAXCONN);
    set_nonblocking(tcp_fd);

    // --- 2. SETUP UDP SOCKET (The Drive-Thru for Media) ---
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd == -1) return 1;

    struct sockaddr_in udp_addr;
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(UDP_PORT);
    
    bind(udp_fd, (struct sockaddr*)&udp_addr, sizeof(udp_addr));
    set_nonblocking(udp_fd); // Notice: No listen() or accept() needed!

    // --- 3. SETUP EPOLL (The Host) ---
    int epoll_fd = epoll_create1(0);
    ThreadPool pool(4, queue, epoll_fd);
    struct epoll_event event, events[MAX_EVENTS];
    
    // Watch TCP socket
    event.events = EPOLLIN; 
    event.data.fd = tcp_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_fd, &event);

    // Watch UDP socket
    event.events = EPOLLIN | EPOLLET; 
    event.data.fd = udp_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_fd, &event);

    std::cout << "TCP Server listening on port " << TCP_PORT << "...\n";
    std::cout << "UDP Server listening on port " << UDP_PORT << "...\n";

    // --- 4. THE INFINITE WATCH LOOP ---
    while (true) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == tcp_fd) {
                // EVENT: New TCP Connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(tcp_fd, (struct sockaddr*)&client_addr, &client_len);
                
                set_nonblocking(client_fd);
                
                event.events = EPOLLIN | EPOLLET; 
                event.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                
                pool.add_client(client_fd);
                std::cout << "New TCP client connected! Socket FD: " << client_fd << "\n";
                
            } else if (events[i].data.fd == udp_fd) {
                // EVENT: Incoming UDP Packet
                Task new_task;
                new_task.client_fd = udp_fd; // Pass the UDP server socket itself
                new_task.is_udp = true;      // Flag it as UDP for the worker thread
                
                queue.push(new_task);
                
            } else {
                // EVENT: Existing TCP Chat Message
                Task new_task;
                new_task.client_fd = events[i].data.fd;
                new_task.is_udp = false; // Flag it as TCP
                
                queue.push(new_task);
            }
        }
    }
    
    close(tcp_fd);
    close(udp_fd);
    return 0;
}