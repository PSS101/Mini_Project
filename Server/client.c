#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<string.h>

int server_fd,client_fd;
struct sockaddr_in server_addr,client_addr;
int caddr = sizeof(client_addr);
int n=0;
char buf[256]={0};
char wbuf[256] ={0};
void line(){
	printf("--------------------------------\n");
	}



void send_server(char buf[]){
	int n = strlen(buf);
	if(send(client_fd,buf,n,0)<0){
		perror("error to wrte\n");
		exit(1);
	}
}

void read_server(){
	if((n=recv(client_fd,buf,sizeof(buf)-1,0))>0){
                         buf[n] = '\0';
                         printf("Server: %s\n",buf);
	}
}

void read_std(){
	if((n=read(0,buf,sizeof(buf)-1))>0){
                       buf[n] = '\0';
         }


}
int main(int argc,char* argv[]){
	if(argc<2){
		exit(1);
	}

	if((client_fd = socket(AF_INET,SOCK_STREAM,0))<0){
		perror("error creating socket");	
		exit(1);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	if(connect(client_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
		perror("connect error\n");
		exit(1);
	}
	printf("connected to server %s\n",argv[1]);
	line();
	

	for(;;){
		memset(buf,0,sizeof(buf));
		read_std();
		send_server(buf);
		read_server();
		
	}
close(client_fd);
//close(server_fd);
return 0;
}
