#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define HTTP_OK "HTTP/1.1 200 OK\r\n\r\n"
#define BUFFER_SIZE 1024

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);


	int server_fd, client_addr_len, client_fd, bytes_recieved, bytes_sent;
	unsigned char buffer[BUFFER_SIZE];
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	// // Since the tester restarts your program quite often, setting REUSE_PORT
	// // ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEPORT failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
	};

	if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
	if (client_fd == -1) {
		printf("client connection failed\n");
		return 1;
	}

	bytes_recieved = read(client_fd, buffer, BUFFER_SIZE);
	printf("recieved rquest (%d bytes): %s\n", bytes_recieved, buffer);

	bytes_sent = send(client_fd, HTTP_OK, strlen(HTTP_OK), 0);
	if (bytes_sent == -1) {
		printf("failed to send HTTP_OK\n");
		return 1;
	}
	printf("successfully sent (%d bytes): %s\n", bytes_sent, HTTP_OK);
	printf("Client connected\n");

	close(client_fd);
	close(server_fd);

	return 0;
}
