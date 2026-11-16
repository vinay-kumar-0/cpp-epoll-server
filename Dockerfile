FROM ubuntu:latest
RUN apt-get update && apt-get install -y g++ make
WORKDIR /app
COPY . .
RUN g++ -o server server.cpp
EXPOSE 8080
CMD ["./server"]