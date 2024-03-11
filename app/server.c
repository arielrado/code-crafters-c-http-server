#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define HTTP_OK "HTTP/1.1 200 OK\r\n\r\n"
#define HTTP_NOT_FOUND "HTTP/1.1 404 NOT FOUND\r\n\r\n"
#define BUFFER_SIZE 1024

typedef struct {
	char method[8];
	char path[256];
	char version[16];
} HttpRequest;

HttpRequest* new_request(const char* method, const char* path, const char* version) {
	HttpRequest* request = malloc(sizeof(HttpRequest));
	strcpy(request->method, method);
	strcpy(request->path, path);
	strcpy(request->version, version);
	return request;
}

HttpRequest* parse_request(char* buffer) {
	if (strlen(buffer) == 0) {
		return NULL;
	}
	char* start_line = strtok(buffer, "\r\n\r\n");
	char* method = strtok(start_line, " ");
	char* path = strtok(NULL, " ");
	char* version = strtok(NULL, " ");

	if (method == NULL || path == NULL || version == NULL)
		return NULL;

	return new_request(method, path, version);
}

int count_tokens(char* str, char* token) {
	int count = 0;
	char* ptr = str;

	while(ptr!='\0'){
		if(&ptr == token){
			count++;
		}
		ptr++;
	}
}

char** parse_path(char* path) {
	int separator_count = count_tokens(path, "/");
	char path_components[separator_count];
	for (int i = 0; i < separator_count; i++) {
		//TODO: use strtok to get components
	}
	
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);


	int server_fd, client_addr_len, client_fd, bytes_recieved, bytes_sent;
	char buffer[BUFFER_SIZE];
	struct sockaddr_in client_addr;
	HttpRequest* request;

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
	printf("Client connected\n");

	bytes_recieved = read(client_fd, buffer, BUFFER_SIZE);
	printf("recieved request (%d bytes): %s\n", bytes_recieved, buffer);

	request = parse_request(buffer);
	if (request == NULL) {
		printf("null request!");
		return 1;
	}

	if (strcmp(request->path, "/") == 0) {
		bytes_sent = send(client_fd, HTTP_OK, strlen(HTTP_OK), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, HTTP_OK);
	}
	else {
		bytes_sent = send(client_fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, HTTP_NOT_FOUND);
	}

	if (bytes_sent == -1) {
		printf("failed to send HTTP_OK\n");
		return 1;
	}
	free(request);
	close(client_fd);
	close(server_fd);

	return 0;
}