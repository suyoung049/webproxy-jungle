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
#include <stdlib.h>
#include <string.h>

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct node{
  char *key;
  char *value;
  struct node *prev;
  struct node *next;
} cache_node_line;

typedef struct cache
{
  cache_node_line *root;
  cache_node_line *tail;
  int size;
} cache;


cache *new_cache();
void doit(int fd);
void free_node(cache_node_line *free_node);
int insert_cache(cache *target_cache, char *key, char *value);
int find_cache(cache *target_cache, char *key, char *buf);
void *thread(void *vargp);
void make_http_header(char *server_buf, char *host, int port, char *uri, rio_t *rio_server);
int parse_uri(const char *uri, char *filename, char *host, char *port, char *parsed_uri); 
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


static cache *proxy_cache;

cache *new_cache()
{
  cache *c = malloc(sizeof(cache));
  c->root = NULL;
  c->tail = NULL;
  c->size = 0;
  return c;
}

cache_node_line *new_cache_node(char *key, char*value)
{
    cache_node_line *node = malloc(sizeof(cache_node_line));
    node->key = malloc(strlen(key) + 1);
    strcpy(node->key, key);
    node->value = malloc(strlen(value) + 1);
    strcpy(node->value, value);
    return node;
}

int main(int argc, char **argv) {
  int listenfd, *connfdp;
  // char hostname[MAXLINE], port[MAXLINE];
  // 전역변수
  proxy_cache = new_cache();
  pthread_t tid;
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
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);
    Pthread_create(&tid, NULL, thread, connfdp);
    
  }
}
void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(Pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

void doit(int fd) 
{   
    char client_buf[MAXLINE],server_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE], host[MAXLINE], port[MAXLINE], parsed_uri[MAXLINE];
    int is_static;
    rio_t rio_client, rio_server;
  

    /* Read request line and headers from client */
    Rio_readinitb(&rio_client, fd);
    if (!Rio_readlineb(&rio_client, client_buf, MAXLINE))  
        return;
    
    printf("@@@@@@@@@@@@@%s", client_buf);
    sscanf(client_buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {  // GET 요청만 처리한다
    printf("Proxy does not implement the method\n");
    return; 
  }

    /* Parse URI */
    parse_uri(uri, filename, host, port, parsed_uri);
    char data[MAX_OBJECT_SIZE] = "";
    printf("host = <%s>, port =<%s>, url =<%s>, data =<%s>\n", host, port, uri, data);

    int c;
    if(c = find_cache(proxy_cache, uri, data)){
        printf("uri = %s\n", uri);
        printf("result find = %d", c);
        printf("=======캐시!===========\n");
        Rio_writen(fd, data, MAXLINE);
    }else{


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
    int cache_check = 1;
    size_t n;
    while ((n = Rio_readlineb(&rio_server, server_buf, MAXLINE)) > 0) {
        printf("%s", server_buf);
        Rio_writen(fd, server_buf, n);

        if(strlen(data) + strlen(server_buf) < MAX_OBJECT_SIZE){
            strcat(data, server_buf);
        }else{
            cache_check = 0;
        }
    }

    /* Clean up */
    Close(serverfd);
    if (cache_check == 1){
        insert_cache(proxy_cache, uri, data);
    }
}
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

int parse_uri(const char *uri, char *filename, char *host, char *port, char *parsed_uri) {
    char *p;
    // Check if URI starts with "http://"
    if (strstr(uri, "http://") != uri) {
        return 0;
    }
    // Extract host and port from URI
    char *host_start = uri + strlen("http://");
    char *port_start = strstr(host_start, ":");
    char *path_start = strstr(host_start, "/");
    strcpy(parsed_uri, path_start);
    p = strchr(port_start, '/');
    if (p != NULL) {
        *p = '\0'; // Cut the string at the '/'
    }
    
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

int find_cache(cache *target_cache, char *key, char *buf){
  cache_node_line *start = target_cache->root;
  printf("start = <%s>\n", start);
  while (start != NULL)
  {
    if(strcmp(start->key, key) == 0){
      strcpy(buf, start->value);
      printf("#####key = %s\n", key);
      // 캐시메모리에 저장되어있는 노드일시, 삭제후 맨처음에 삽입
      insert_cache(target_cache, start->key, start->value);
      delete_node(target_cache, start);
      return 1;
    }
    start = start->next;
  }
  printf("++++++++++++++++++");
  return 0;
}

int insert_cache(cache *target_cache, char *key, char *value)
{
    printf("insert cache\n");

    if (target_cache->size + strlen(value) > MAX_CACHE_SIZE)
    {   // 사이즈 초과시 맨 끝 노드 삭제
        delete_node(target_cache, target_cache->tail);
    }
    target_cache->size += strlen(value);

    cache_node_line *new_node = new_cache_node(key, value);
    // 새 노드는 맨앞에 들어갈 수 밖에 없기 때문에 새노드의 앞은 원래 캐시의 처음
    new_node->next = target_cache->root;

    // 캐시의 앞이 NULL 값이 아니라면, 원래 앞에 있던 노드의 전은 새 노드
    if (target_cache->root != NULL){
        target_cache->root->prev = new_node;
    // 캐시의 앞이 비워있다면 끝노드 역시 새노드
    } else {
        target_cache->tail = new_node;
    }
    // 처음 노드 새노드로 교체
    target_cache->root = new_node;
}

int delete_node(cache *target_cache, cache_node_line *delete_node){
    // 삭제 노드의 전 노드가 있을시
    if (delete_node->prev != NULL){
        delete_node->prev->next = delete_node->next;
    // 삭제 노드이 전 노드가 없을시
    } else {
        target_cache->root = delete_node->next;  
    }

    if (delete_node->next != NULL){
        delete_node->next->prev = delete_node->prev;
    } else {
        target_cache->tail = delete_node->prev;
    }

    free_node(delete_node);
}

void free_node(cache_node_line *free_node){
    free(free_node->key);
    free(free_node->value);
    free(free_node);
}

   

