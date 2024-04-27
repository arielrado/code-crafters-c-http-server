#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#define HTTP_OK_EMPTY "HTTP/1.1 200 OK\r\n\r\n"
#define HTTP_OK "HTTP/1.1 200 OK\r\n"
#define HTTP_NOT_FOUND "HTTP/1.1 404 NOT FOUND\r\n\r\n"
#define PLAIN_TEXT "Content-Type: text/plain\r\n"
#define ECHO_LEN 6
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

int count_tokens(char* str, char token) {
	int count = 0;
	char* ptr = str;

	while (*ptr != '\0') {
		if (*ptr == token) {
			count++;
		}
		ptr++;
	}
	return count;
}

char** parse_path(char* path) {
	printf("path: %s\n", path);
	int separator_count = count_tokens(path, '/');
	printf("separator count: %d\n", separator_count);
	char** path_components = (char**)malloc(separator_count * sizeof(char));
	if (path_components == NULL) {
		printf("malloc failed!");
		return NULL;
	}
	path_components[0] = strtok(path, "/");
	for (int i = 1; i < separator_count; i++) {
		path_components[i] = strtok(NULL, "/");
	}
	return path_components;
}

bool generate_echo_response(HttpRequest* req, char* buffer) {
	int i = 0;
	strcpy(buffer, HTTP_OK);
	i=strlen(buffer);
	strcpy(buffer+i, PLAIN_TEXT);
	i=strlen(buffer);
	char* string;												// len(/echo/)=6
	if(0 > asprintf(&string, "Content-Length: %d\r\n\r\n", strlen(req->path+ECHO_LEN))) return false;
	strcpy(buffer+i, string);
	i=strlen(buffer);
	free(string);
	strcpy(buffer+i, req->path+ECHO_LEN);
	i=strlen(buffer);
	strcpy(buffer+i, "\r\n\r\n");
	return true;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);


	int server_fd, client_addr_len, client_fd, bytes_recieved, bytes_sent;
	char buffer[BUFFER_SIZE];
	struct sockaddr_in client_addr;
	HttpRequest* request;
	char** path_components;

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
	printf("parsed request: %s %s %s\n", request->method, request->path, request->version);
	if (request == NULL) {
		printf("null request!");
		return 1;
	}

	path_components = parse_path(request->path);
	if (strlen(path_components[0]) == 0) {
		bytes_sent = send(client_fd, HTTP_OK_EMPTY, strlen(HTTP_OK_EMPTY), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, HTTP_OK_EMPTY);
	}
	else if (strcmp(path_components[0], "echo") == 0) {
		printf("echo request\n");
		if(!generate_echo_response(request, buffer)) return 1;
		bytes_sent = send(client_fd, buffer, strlen(buffer), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, buffer);
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