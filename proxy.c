#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";

struct Uri
{
    char host[MAXLINE];
    char port[MAXLINE]; 
    char path[MAXLINE]; 
};


void *thread(void *vargp);
void doit(int client_fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, struct Uri *uri_data);
void send_request(struct Uri *uri_data,int client_fd,char* uri);


//part of the function is copied from tiny.c
int main(int argc, char **argv)
{
    pthread_t tid;
    char hostname[MAXLINE], port[MAXLINE];
    struct sockaddr_storage clientaddr;
    socklen_t clientlen  = sizeof(clientaddr);
    int *connfd,listenfd;

    
    /* Check command line args */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }
    
    //Initialize the cache space
    init_cache();

    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
    connfd = Malloc(sizeof(int));
	*connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    Pthread_create(&tid, NULL, thread, connfd);  
                                    
    }
    Close(listenfd);
    
    //free the cache space when proxy closed
    free_cache();

    return 0;
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);                                             
    Close(connfd);  
    return NULL;
}



/* $begin doit */
void doit(int client_fd) 
{   
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    rio_t client_rio;
 
    struct Uri *uri_data = (struct Uri *)malloc(sizeof(struct Uri));

    
    Rio_readinitb(&client_rio, client_fd);
    

    //check the method
    if (!Rio_readlineb(&client_rio, buf, MAXLINE))  
        return;
    sscanf(buf, "%s %s %s", method, uri, version);       
    if (strcasecmp(method, "GET")) {                     
        clienterror(client_fd, method, "501", "Not Implemented",
                    "Proxy Server does not implement this method");
        return;
    }

    //if there is cache, read the cache, otherwise send request 
    int ret = read_cache(uri,client_fd);
    if(ret == 1){
        return;
    }
  
    //parse uri, write into uri_data
    parse_uri(uri, uri_data);


    //send request to client
    send_request(uri_data,client_fd,uri);
   
    
}
/* $end doit */


void parse_uri(char *uri, struct Uri *uri_data)
{   //example uri http://www.cmu.edu:8080/hub/index.html
    //search for first "//"  the position of host
    char *hostPosition = strstr(uri, "//");
    hostPosition += 2;
    

    //search for : if there is port write in uri_data
    //otherwise use default port 80
    char *portPosition = strstr(hostPosition, ":");
    if (portPosition != NULL)
    {
         int tmp;
          sscanf(portPosition + 1, "%d%s", &tmp, uri_data->path);
          sprintf(uri_data->port, "%d", tmp);
        *portPosition = '\0';
      }
       else
       {
          char *pathPosition = strstr(hostPosition, "/");
         if (pathPosition != NULL)
        {
             strcpy(uri_data->path, pathPosition);
            strcpy(uri_data->port, "80");
            *pathPosition = '\0';
         }
    }
       strcpy(uri_data->host, hostPosition);
}




void send_request(struct Uri *uri_data,int client_fd, char *uri){
    int server_fd;
    rio_t server_rio;
    char buf[MAXLINE];
     server_fd = Open_clientfd(uri_data->host, uri_data->port);
    if(server_fd<0){
        printf("connection failed\n");
        return;
    }
    
    Rio_readinitb(&server_rio, server_fd);

    
    //add header
    sprintf(buf, "GET %s HTTP/1.0\r\n", uri_data->path);
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Host: %s\r\n", uri_data->host);
    Rio_writen(server_fd, buf, strlen(buf));
    Rio_writen(server_fd, (char *) user_agent_hdr, strlen(user_agent_hdr));
    sprintf(buf, "Connection: close\r\n");
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "Proxy-Connection: close\r\n");
    Rio_writen(server_fd, buf, strlen(buf));
    sprintf(buf, "\r\n");
    Rio_writen(server_fd, buf, strlen(buf));
    

    //check the size and write into cache
    int filesize, size = 0;
    char cacheData[MAX_OBJECT_SIZE];
    while ((filesize = Rio_readlineb(&server_rio, buf, MAXLINE))) {
        if (size <= MAX_OBJECT_SIZE) {
            memcpy(cacheData+size,buf,filesize);
            size += filesize;
        }
        printf("proxy received %d bytes\n",filesize);
        Rio_writen(client_fd, buf, filesize); 
    }

    if(size <= MAX_OBJECT_SIZE){
        write_cache(uri,cacheData,size);
    }
    Close(server_fd);
}



// part of this function is copied from tiny.c
/* $begin clienterror */
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
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>Proxy server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */

