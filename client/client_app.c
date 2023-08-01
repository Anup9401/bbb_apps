
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<netinet/in.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>

int main(int argc, char *argv[]){

	if(argc!=3){
		printf("Usage : ./client_app portNumber serverAddress\n");
		exit(1);
	}

	char buf[256], key[5];
	int sfd=socket(AF_INET,SOCK_STREAM,0), readBytes, num;
	if(sfd<0){
		perror("socket");
		exit(1);
	}

	printf("Socket created...\n");
	struct sockaddr_in cln;
	cln.sin_family=AF_INET;
	cln.sin_port=htons(atoi(argv[1]));
	cln.sin_addr.s_addr=inet_addr(argv[2]);
	int retConnect=connect(sfd,(struct sockaddr *)&cln, sizeof(cln));

	if(retConnect<0){
		perror("connect");
		exit(1);
	}

	printf("Connection accepted\n");

	while(1){

		readBytes=recv(sfd,buf,200,0);
		if(readBytes==-1){
			printf("Connection Closed\n");
			break;
		}
		if(readBytes==0){
			printf("No data yet, try again...\n");
		}
		else{
			for(int i=0;i<readBytes;i++){
				printf("%c",buf[i]);
			}
		}
		if(strstr(buf,"key")||(readBytes==0)){
			memset(buf,0,200*sizeof(char));
			printf("Enter key or Enter 0 to terminate\n");
			scanf("%d", &num);
			if(num==0){
				break;
			}
			sprintf(key,"%d\n",num);
			if(send(sfd, key, 4, 0)==-1){
				printf("Connection Closed\n");
				break;
			}
		}

	}
}

