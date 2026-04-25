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

pid_t pid;
void line(){
	printf("--------------------------------\n");
	}

void send_client(char buf[]){
	int n = strlen(buf);
	if(send(client_fd,buf,n,0)<0){
		perror("error to wrte\n");
		exit(1);
	}
}

int get_cmd(char**cmd,char buf[]){
	char* t;
	int i=0;
	t = strtok(buf," ");
	cmd[0] = t;
	while(t!=NULL){
		cmd[i] = t;
		t = strtok(NULL," ");
		i++;
	}
	for(int j=0;j<i;j++){
		if(j!=i-1){printf("%s,",cmd[j]);}
		else{printf("%s",cmd[j]);}
	}
	printf("\n");
	return i;	
	
}

int main(int argc,char* argv[]){
	if(argc<2){
		exit(1);
	}

	if((server_fd = socket(AF_INET,SOCK_STREAM,0))<0){
		perror("error creating socket");	
		exit(1);
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	server_addr.sin_addr.s_addr = INADDR_ANY;
	
	int opt = 1;
	setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	
	if(bind(server_fd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
		perror("bind error\n");
		exit(1);
	}
	printf("server binded to %s\n",argv[1]);
	line();
	if(listen(server_fd,5)<0){
		perror("listen error\n");
		exit(1);
	}
	
	printf("server listening on port: %s\n",argv[1]);
	line();

	for(;;){
		caddr = sizeof(client_addr);
		if((client_fd = accept(server_fd,(struct sockaddr*)&client_addr,&caddr))<0){
			perror("failed to accept\n");
			exit(1);
		}
		
		if((pid = fork())<0){
			exit(1);
		}
		else if(pid==0){
			char * cmd[10];
			close(server_fd);
			printf("client accpeted on: %s\n",inet_ntoa(client_addr.sin_addr));
			line();		
			while((n=recv(client_fd,buf,sizeof(buf)-1,0))>0){
				buf[n] = '\0';
				buf[strcspn(buf,"\r\n")]=0;
				printf("Client: %s\n",buf);
				int idx  =get_cmd(cmd,buf);
				send_client("asd");
				if(strcmp(buf,"1234")==0){
					printf("same string\n");
				}
			}
			if(n<0){
				exit(1);
			}
			else{
				printf("Client connection closed");
			}
		}
		else{
			close(client_fd);
		}
		
	}
close(client_fd);
close(server_fd);
return 0;
}
