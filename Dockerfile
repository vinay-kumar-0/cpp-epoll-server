FROM ubuntu:latest
RUN apt-get update && apt-get install -y g++ make
WORKDIR /app
COPY . .
RUN g++ -o server server.cpp # Adjust to your actual compile command
EXPOSE 8080 # The port your epoll server binds to
CMD ["./server"]