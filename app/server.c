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
	char host[32];
	char user_agent[256];
} HttpRequest;

HttpRequest* new_request(const char* method,
							const char* path,
							const char* version,
							const char* host,
							const char* user_agent) {
	HttpRequest* request = malloc(sizeof(HttpRequest));
	strcpy(request->method, method);
	strcpy(request->path, path);
	strcpy(request->version, version);
	strcpy(request->host, host);
	strcpy(request->user_agent, user_agent);
	return request;
}

HttpRequest* parse_request(char* buffer) {
	if (strlen(buffer) == 0) {
		return NULL;
	}
	char* rest = buffer;
	char* first_line = strtok_r(buffer, "\r\n",&rest);
	char* second_line = strtok_r(NULL, "\r\n",&rest);
	char* third_line = strtok_r(NULL, "\r\n",&rest);

	char* method = strtok_r(first_line, " ",&rest);
	char* path = strtok_r(NULL, " ",&rest);
	char* version = strtok_r(NULL, " ",&rest); 

	// printf("second line: %s\n", second_line);
	// printf("third line: %s\n", third_line);
	char* host = (second_line)?second_line+strlen("Host: "):"\0";
	char* user_agent = (third_line)?third_line+strlen("User-agent: "):"\0";
	// printf("Host: %s\nUser-agent: %s\n", host, user_agent);
	if (method == NULL || path == NULL || version == NULL)
		return NULL;
	return new_request(method, path, version, host, user_agent);
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

bool isEcho(char* path) {
	return strncmp(path, "/echo/", 6) == 0;
}

char** parse_path(char* path) {
	printf("path: %s\n", path);
	int separator_count = count_tokens(path, '/');
	printf("separator count: %d\n", separator_count);
	char** path_components = (char**)malloc(separator_count * sizeof(char*));
	if (path_components == NULL) {
		printf("malloc failed!\n");
		return NULL;
	}
	path_components[0] = strtok(path, "/");
	for (int i = 1; i < separator_count; i++) {
		path_components[i] = strtok(NULL, "/");
	}
	return path_components;
}

bool generate_user_agent_response(HttpRequest* req, char* buffer) {
	int i = 0;
	strcpy(buffer, HTTP_OK);
	i=strlen(buffer);
	strcpy(buffer+i, PLAIN_TEXT);
	i=strlen(buffer);
	char* string;
	if(0 > asprintf(&string, "Content-Length: %lu\r\n\r\n", strlen(req->user_agent))) return false;
	strcpy(buffer+i, string);
	i=strlen(buffer);
	free(string);
	strcpy(buffer+i, req->user_agent);
	i=strlen(buffer);
	strcpy(buffer+i, "\r\n\r\n");
	return true;
}

bool generate_echo_response(HttpRequest* req, char* buffer) {
	int i = 0;
	strcpy(buffer, HTTP_OK);
	i=strlen(buffer);
	strcpy(buffer+i, PLAIN_TEXT);
	i=strlen(buffer);
	char* string;												// len(/echo/)=6
	if(0 > asprintf(&string, "Content-Length: %lu\r\n\r\n", strlen(req->path+ECHO_LEN))) return false;
	strcpy(buffer+i, string);
	i=strlen(buffer);
	free(string);
	strcpy(buffer+i, req->path+ECHO_LEN);
	i=strlen(buffer);
	strcpy(buffer+i, "\r\n\r\n");
	return true;
}

int handle_connection(int client_fd){
	char buffer[BUFFER_SIZE];
	int bytes_recieved, bytes_sent;
	char** path_components;
	HttpRequest *request;
	bytes_recieved = read(client_fd, buffer, BUFFER_SIZE);
	printf("recieved request (%d bytes): %s\n", bytes_recieved, buffer);

	request = parse_request(buffer);
	printf("parsed request: %s %s %s\n", request->method, request->path, request->version);
	if (request == NULL) {
		printf("null request!");
		return 1;
	}

	if(isEcho(request->path)) {
		printf("echo request\n");
		if(!generate_echo_response(request, buffer)) return 1;
		bytes_sent = send(client_fd, buffer, strlen(buffer), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, buffer);
	} else if (strcmp(request->path, "/user-agent") == 0) {
		// send user-agent
		if(!generate_user_agent_response(request, buffer)) return 1;
		bytes_sent = send(client_fd, buffer, strlen(buffer), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, buffer);
	} else {
		path_components = parse_path(request->path);
		printf("path components[0]: %s\n", path_components[0]);
		if (path_components[0]==NULL) {
			printf("empty path\n");
			bytes_sent = send(client_fd, HTTP_OK_EMPTY, strlen(HTTP_OK_EMPTY), 0);
			printf("successfully sent (%d bytes): %s\n", bytes_sent, HTTP_OK_EMPTY);
		} else {
		bytes_sent = send(client_fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, HTTP_NOT_FOUND);
		}
	}
		

	if (bytes_sent == -1) {
		printf("failed to send HTTP_OK\n");
		return 1;
	}
	free(request);
	close(client_fd);
	printf("Client disconnected\n");
	return 0;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);


	int server_fd, client_fd;
	socklen_t client_addr_len;
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

	const int connection_backlog = 126;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	client_addr_len = (socklen_t)sizeof(client_addr);

	while(true){
		printf("Waiting for a client to connect...\n");

		client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
		if (client_fd == -1) {
			printf("client connection failed\n");
			return 1;
		}
		printf("Client connected\n");

		if(handle_connection(client_fd, buffer)) return 1;
	}
	close(server_fd);

	return 0;
}