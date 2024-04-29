#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <paths.h>
#include <sys/stat.h>

#define HTTP_OK_EMPTY "HTTP/1.1 200 OK\r\n\r\n"
#define HTTP_OK "HTTP/1.1 200 OK\r\n"
#define HTTP_201 "HTTP/1.1 201 Created\r\n\r\n"
#define HTTP_NOT_FOUND "HTTP/1.1 404 NOT FOUND\r\n\r\n"
#define PLAIN_TEXT "Content-Type: text/plain\r\n"
#define OCTET_STREAM "Content-Type: application/octet-stream\r\n"
#define ECHO_LEN 6
#define BUFFER_SIZE 1024
#define PATH_SIZE 256

typedef struct {
	char method[8];
	char path[256];
	char version[16];
	char host[32];
	char user_agent[256];
	int content_length;
	char accept_encoding[16];
	char body[1024];
} HttpRequest;

char* directory = "\0";

HttpRequest* new_request(const char* method,
							const char* path,
							const char* version,
							const char* host,
							const char* user_agent,
							const int content_length,
							const char* accept_encoding,
							const char* body) {
	HttpRequest* request = malloc(sizeof(HttpRequest));
	strcpy(request->method, method);
	strcpy(request->path, path);
	strcpy(request->version, version);
	strcpy(request->host, host);
	strcpy(request->user_agent, user_agent);
	request->content_length = content_length;
	strcpy(request->accept_encoding, accept_encoding);
	strcpy(request->body, body);
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
	char* fourth_line = strtok_r(NULL, "\r\n",&rest);
	char* fifth_line = strtok_r(NULL, "\r\n",&rest);
	char* body = strtok_r(NULL, "\r\n",&rest);

	char* method = strtok_r(first_line, " ",&rest);
	char* path = strtok_r(NULL, " ",&rest);
	char* version = strtok_r(NULL, " ",&rest); 

	// printf("second line: %s\n", second_line);
	// printf("third line: %s\n", third_line);
	char* host = (second_line)?second_line+strlen("Host: "):"\0";
	char* user_agent = (third_line)?third_line+strlen("User-agent: "):"\0";
	int content_length = atoi((fourth_line)?fourth_line+strlen("Content-Length: "):"0");
	char* accept_encoding = (fifth_line)?fifth_line+strlen("Accept-Encoding: "):"\0";
	body = (body)?body:"\0";
	printf("body: %s\n", body);
	if (method == NULL || path == NULL || version == NULL)
		return NULL;
	return new_request(method, path, version, host, user_agent,content_length,accept_encoding,body);
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

bool isFiles(char* path) {
	return strncmp(path, "/files/", 7) == 0;
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

FILE* generate_file_resonse(HttpRequest* req, char* buffer) {
	if(directory[0]=='\0'){
		printf("no directory\n");
		return NULL;
	}
	char path[PATH_SIZE], actualpath[PATH_SIZE];
	int i=0;
	strcpy(path, directory);
	i=strlen(path);
	strcpy(path+i, req->path+strlen("/files/"));
	printf("path: %s\n", path);
	if(!realpath(path, actualpath)){
		printf("bad path %s\n",path);
		return NULL;
	}
	FILE* file = fopen(actualpath, "r");
	if(!file){
		printf("error opening file\n");
		return NULL;
	}
	
	struct stat st;
	if(stat(path, &st)){
		printf("error getting file stats\n");
		return NULL;
	}

	//place the header in buffer
	i=0;
	strcpy(buffer, HTTP_OK);
	i=strlen(buffer);
	strcpy(buffer+i, OCTET_STREAM);
	i=strlen(buffer);
	char* string;
	if(0 > asprintf(&string, "Content-Length: %lld\r\n\r\n", st.st_size)) return false;
	strcpy(buffer+i, string);
	i=strlen(buffer);
	free(string);
	return file;
}

bool handle_post_request(HttpRequest* req, char* buffer) {
	if(directory[0]=='\0'){
		printf("no directory\n");
		return false;
	}
	char path[PATH_SIZE];
	int i=0;
	strcpy(path, directory);
	i=strlen(path);
	strcpy(path+i, req->path+strlen("/files/"));
	printf("path: %s\n", path);
	FILE* file = fopen(path, "w");
	if(!file){
		printf("error opening file\n");
		return false;
	}
	fwrite(req->body, 1, req->content_length, file);
	fclose(file);
	printf("successfully wrote to file (%d bytes)\n", req->content_length);
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

	if (strcmp(request->method, "POST") == 0) {
		//save the body to specified file
		printf("POST request\n");
		if(!isFiles(request->path)){
			printf("invalid path\n");
			send(client_fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
		} else {
			if(!handle_post_request(request, buffer)){
				printf("error handling post request\n");
				send(client_fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
			} else {
				bytes_sent = send(client_fd, HTTP_201, strlen(HTTP_201), 0);
				printf("successfully sent (%d bytes): %s\n", bytes_sent, HTTP_201);
			}
		}
	} else if(isEcho(request->path)) {
		printf("echo request\n");
		if(!generate_echo_response(request, buffer)) return 1;
		bytes_sent = send(client_fd, buffer, strlen(buffer), 0);
		printf("successfully sent (%d bytes): %s\n", bytes_sent, buffer);
	} else if (isFiles(request->path)) {
		printf("files request\n");
		FILE* file = generate_file_resonse(request, buffer);
		if(!file) {
			send(client_fd, HTTP_NOT_FOUND, strlen(HTTP_NOT_FOUND), 0);
		} else {
			//send the header
			bytes_sent = write(client_fd, buffer, strlen(buffer));
			printf("successfully sent header(%d bytes): %s\n", bytes_sent, buffer);
			//send the file
			while((bytes_sent=fread(buffer, 1, BUFFER_SIZE, file))>0){
				printf("sending %d bytes\n", bytes_sent);
				bytes_sent = write(client_fd, buffer, bytes_sent);
				printf("successfully sent (%d bytes)\n", bytes_sent);
			}
			fclose(file);
		}
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

void *t_handle_connection(void* p_client_fd) {
	int client_fd = *(int*)p_client_fd;
	free(p_client_fd);
	handle_connection(client_fd);
	return NULL;
}

int main(int argc, char* argv[]) {
	// Disable output buffering
	setbuf(stdout, NULL);

	if (argc>=3 && (strcmp(argv[1],"--directory")==0)) {
		directory=argv[2]; //dierctory is global
		printf("directory: %s\n", directory);
	}

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

	const int connection_backlog = 128;
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

		pthread_t thread;
		int* p_client_fd = malloc(sizeof(int));
		*p_client_fd = client_fd;
		pthread_create(&thread, NULL, t_handle_connection, p_client_fd);
	}
	close(server_fd);

	return 0;
}