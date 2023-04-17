/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"
#include <unistd.h>
#include <stdio.h>

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *host, char *port); 
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen);  
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   
    Close(connfd); 
  }
}

void doit(int fd) 
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE], host[MAXLINE], port[MAXLINE];
    int is_static;
    struct stat sbuf;
    rio_t rio_client, rio_server;

    /* Read request line and headers from client */
    Rio_readinitb(&rio_client, fd);
    if (!Rio_readlineb(&rio_client, buf, MAXLINE))  
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {  // GET 요청만 처리한다
    printf("Proxy does not implement the method\n");
    return; 
  }

    /* Parse URI */
    parse_uri(uri, filename, host, port);
    sprintf(buf, "%s %s %s\r\n", method, uri, version);
    printf("==========%s\n", host);
    printf("==========%s\n", port);
    printf("==========%s\n", buf);

    
    
    /* Open a connection to the external server */
    int serverfd = Open_clientfd(host, port);
    if (serverfd < 0) {
        printf("Failed to connect to server\n");
        return;
    }
    
    // 1. 개행별로 쓴 큰 버퍼 하나 선언
    // while(strcmp(buf, "\r\n")){
    //     Rio_readlineb(&rio_client, buf, MAXLINE);
    //     Rio_writen(, buf, strlen(buf));
        
    // }

    // 선언한 버퍼 사용
    Rio_writen(serverfd, buf, strlen(buf));
    /* Forward the request to the server */
    Rio_readinitb(&rio_server, serverfd);
    
    // while (Rio_readlineb(&rio_client, buf, MAXLINE)) {
    //     printf("%s", buf);
    //     Rio_writen(serverfd, buf, strlen(buf));
    //     if (strcmp(buf, "\r\n") == 0) {
    //         break;
    //     }
    // }

    
    // 여기서 부터는 그냥 buf
    /* Forward the response from the server to the client */
    while (Rio_readlineb(&rio_server, buf, MAXLINE)) {
        printf("%s", buf);
        Rio_writen(fd, buf, strlen(buf));
    }

    /* Clean up */
    Close(serverfd);
}

void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

// void read_requesthdrs(rio_t *rp)
// {
// 	char buf[MAXLINE];

// 	Rio_readlineb(rp, buf, MAXLINE);
// 	while (strcmp(buf, "\r\n"))
// 	{
// 		Rio_readlineb(rp, buf, MAXLINE);
// 		printf("%s", buf);
// 	}
// 	return;
	
// }

int parse_uri(char *uri, char *filename, char *host, char *port) 
{   char *p;
    // Check if URI starts with "http://"
    if (strstr(uri, "http://") != uri) {
        return 0;
    }
    printf("==========%s\n", uri);
    // Extract host and port from URI
    char *host_start = uri + strlen("http://");
    printf("==========%s\n", host_start);
    char *port_start = strstr(host_start, ":");
    char *path_start = strstr(host_start, "/");
    printf("==========%s\n", port_start);
    printf("==========%s\n", path_start);
    p = strchr(port_start, '/');
    *p = '\0';
    if (port_start && (!path_start || port_start < path_start)) {
        // Port is specified in URI
        strncpy(host, host_start, port_start - host_start);
        host[port_start - host_start] = '\0';
        sscanf(port_start + 1, "%s", port);
    } else {
        // Port is not specified in URI
        char *path_end = path_start ? path_start : uri + strlen(uri);
        strncpy(host, host_start, path_end - host_start);
        host[path_end - host_start] = '\0';
        strcpy(port, "80"); // Default port is 80
    }
    // Extract file path from URI
    if (path_start) {
        strcpy(uri, "/");
        return 1;
    } else {
        strcpy(uri, "/");
        return 1;
    }
}

