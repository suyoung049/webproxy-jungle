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
void make_http_header(char *server_buf, char *host, int port, char *uri, rio_t *rio_server);
int parse_uri(const char *uri, char *filename, char *host, char *port, char *parsed_uri); 
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

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
    char client_buf[MAXLINE],server_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE], host[MAXLINE], port[MAXLINE], parsed_uri[MAXLINE], totalbuf[MAXLINE];
    int is_static;
    rio_t rio_client, rio_server, rio_proxy;

    /* Read request line and headers from client */
    Rio_readinitb(&rio_client, fd);
    if (!Rio_readlineb(&rio_client, client_buf, MAXLINE))  
        return;
    printf("%s", client_buf);
    sscanf(client_buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {  // GET 요청만 처리한다
    printf("Proxy does not implement the method\n");
    return; 
  }

    /* Parse URI */
    parse_uri(uri, filename, host, port, parsed_uri);
    printf("==========%s\n", host);
    printf("==========%s\n", port);
    printf("==========%s\n", uri);
    
    
    /* Open a connection to the external server */
    int serverfd = Open_clientfd(host, port);
    if (serverfd < 0) {
        printf("Failed to connect to server\n");
        return;
    }
    
    sprintf(server_buf, "%s %s %s\r\n", method, uri, version);
    // 선언한 버퍼 사용
    Rio_readinitb(&rio_server, serverfd);
    make_http_header(server_buf, host, port, uri, &rio_client);
    Rio_writen(serverfd, server_buf, strlen(server_buf));
    
    // 여기서 부터는 그냥 buf
    /* Forward the response from the server to the client */
    Rio_readinitb(&rio_server, serverfd);
    size_t n;
    while ((n = Rio_readlineb(&rio_server, server_buf, MAXLINE)) > 0) {
        printf("%s", server_buf);
        Rio_writen(fd, server_buf, n);
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

int parse_uri(const char *uri, char *filename, char *host, char *port, char *parsed_uri) {
    char *p;
    // Check if URI starts with "http://"
    if (strstr(uri, "http://") != uri) {
        return 0;
    }
    // printf("==========%s\n", uri);
    // Extract host and port from URI
    char *host_start = uri + strlen("http://");
    // printf("==========%s\n", host_start);
    char *port_start = strstr(host_start, ":");
    char *path_start = strstr(host_start, "/");
    // printf("==========%s\n", path_start);
    strcpy(parsed_uri, path_start);
    // printf("==========%s\n", parsed_uri);
    p = strchr(port_start, '/');
    if (p != NULL) {
        *p = '\0'; // Cut the string at the '/'
    }
    // printf("==========%s\n", port_start+1);
    // printf("==========%s\n", path_start);
    
    if (port_start && (!path_start || port_start < path_start)) {
        // Port is specified in URI
        strncpy(host, host_start, port_start - host_start);
        host[port_start - host_start] = '\0';
        strcpy(port, port_start+1);
    }
    else {
        // Port is not specified in URI
        char *path_end = path_start ? path_start : uri + strlen(uri);
        strncpy(host, host_start, path_end - host_start);
        host[path_end - host_start] = '\0';
        strcpy(port, "80"); // Default port is 80
    }
    // Extract filename and path from URI
    if (path_start) {
        strcpy(uri, parsed_uri);
        return 1;
    } else {
        strcpy(uri, "/");
        return 1;
    }
    return 1;
}

void make_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio) {
  char buf[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];

  sprintf(http_header, "GET %s HTTP/1.0\r\n", path);

  while(Rio_readlineb(server_rio, buf, MAXLINE) > 0) {
    if (strcmp(buf, "\r\n") == 0) break;

    if (!strncasecmp(buf, "Host", strlen("Host")))  // Host: 
    {
      strcpy(host_hdr, buf);
      continue;
    }

    if (strncasecmp(buf, "Connection", strlen("Connection"))
        && strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection"))
        && strncasecmp(buf, "User-Agent", strlen("User-Agent")))
    {
      strcat(other_hdr, buf);
    }
  }

  if (strlen(host_hdr) == 0) {
    sprintf(host_hdr, "Host: %s:%d\r\n", hostname, port);
  }

  sprintf(http_header, "%s%s%s%s%s%s%s", http_header, host_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n", user_agent_hdr, other_hdr, "\r\n");
  return;
}



   

