#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* IP address and port number */
#define PORT 8181
// 定义端口号，一般情况下 Web 服务器的端口号为 80
#define IP_ADDRESS "192.168.0.8"
//定义服务端的 IP 地址
/* Request a html file base on HTTP */
char *httprequestMsg = "GET /helloworld.html HTTP/1.0 \r\n\r\n" ;
//定义了 HTTP 请求消息，即请求helloworld.html 文件
#define BUFSIZE 8196

void pexit(char * msg) {
    perror(msg);
    exit(1);
}

int main(){
    int i,sockfd;
    char buffer[BUFSIZE];
    static struct sockaddr_in serv_addr;
    printf("client trying to connect to %s and port %d\n",IP_ADDRESS,PORT);
    if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0) //创建客户端 socket
        pexit("socket() error");
    serv_addr.sin_family = AF_INET;
    //设置 Socket 为 IPv4 模式
    serv_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS);// 设置连接服务器的 IP 地址
    serv_addr.sin_port = htons(PORT); // 设置连接服务器的端口号
    /* 连接指定的服务器*/
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) <0) pexit("connect() error");
    /* 当连接成功后，通过 socket 连接通道向服务器端发送请求消息 */
    printf("Send bytes=%d %s\n",strlen(httprequestMsg), httprequestMsg);
    write(sockfd, httprequestMsg, strlen(httprequestMsg));
    /* 通过 socket 连接通道，读取服务器的响应消息；即 helloworld.html 文件内容；如果是在 Web 浏览器中，web 浏览器将根据得到的文件内容，进行 Web 页面渲染*/
    while( (i=read(sockfd,buffer,BUFSIZE)) > 0)write(1,buffer,i);
    /*close the socket*/
    close(sockfd);
    return 0;
}