
#include<syslog.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<fcntl.h>
#include<signal.h>
#include<pthread.h>
#include<stdbool.h>
#include<sys/stat.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include "queue.h"
#include "aesd_ioctl.h"

#define PORT 9000
#define MEM_SIZE 1024
#define FILE_PATH "/dev/aesdchar"

typedef struct {
	pthread_mutex_t *mutex;
	pthread_t tid;
	int fd;
	int nsfd;
	char cln_addr[INET6_ADDRSTRLEN];
	bool thread_complete;
}thread_client;

struct entry {
	thread_client thread_client_param;
	SLIST_ENTRY(entry) entries;
};

typedef struct {
	int num;
	pthread_mutex_t *mutex;
	pthread_t tid;
	int fd;
	int fd_swVal;
	int fd_ledVal;
	int fd_motorVal1;
	int fd_motorVal2;
}thread_switch;

SLIST_HEAD(slisthead, entry) head;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int sfd=-1, fd=-1, fd_dir=-1;
bool srv_terminated=false;
thread_switch *sw1_param=NULL, *sw2_param=NULL;

void closeFile(int fd, int clear){

	if(fd!=-1){
		if(clear){
			if(write(fd, "0", 1)==-1){
				syslog(LOG_ERR,"write() clear");
			}
			if(fdatasync(fd)==-1){
				syslog(LOG_ERR,"fdatasync() clear");
			}
		}
		close(fd);
	}
}

void cleanup(){

	srv_terminated=true;

	while (!SLIST_EMPTY(&head)) {
		struct entry *checkData=NULL;
		checkData = SLIST_FIRST(&head);
		pthread_join(checkData->thread_client_param.tid,NULL);
		SLIST_REMOVE_HEAD(&head, entries);
		free(checkData);
	}
	SLIST_INIT(&head);

	closeFile(fd_dir,0);
	if(sw1_param!=NULL){
		if(sw1_param->tid!=-1){
			pthread_join(sw1_param->tid,NULL);
		}
		closeFile(sw1_param->fd_swVal,0);
		closeFile(sw1_param->fd_ledVal,1);
		closeFile(sw1_param->fd_motorVal1,1);
		closeFile(sw1_param->fd_motorVal2,1);
		free(sw1_param);
	}
	if(sw2_param!=NULL){
		if(sw2_param->tid!=-1){
			pthread_join(sw2_param->tid,NULL);
		}
		closeFile(sw2_param->fd_swVal,0);
		closeFile(sw2_param->fd_ledVal,1);
		closeFile(sw2_param->fd_motorVal1,1);
		closeFile(sw2_param->fd_motorVal2,1);
		free(sw2_param);
	}
	if(sfd!=-1){
		shutdown(sfd,SHUT_RDWR);
		close(sfd);
	}
	if(pthread_mutex_destroy(&mutex)){
		syslog(LOG_ERR,"pthread_mutex_destroy() failed\n");
	}

	closelog();
}

void signalHandler(int signal){

	if(signal==2||signal==15){
		cleanup();
		syslog(LOG_DEBUG,"caught signal, exiting\n");
		exit(0);
	}
}

void fileReadWrite(int fd, char *val, int len, int op){

	lseek(fd,0,SEEK_SET);
	if(op==0){
		if(read(fd, val, len)==-1){
			syslog(LOG_ERR,"read() operation");
			kill(getpid(), 2);
		}

	}
	else if(op==1){
		if(write(fd, val, len)==-1){
			syslog(LOG_ERR,"write() operation");
			kill(getpid(), 2);
		}
		if(fdatasync(fd)==-1){
			syslog(LOG_ERR,"fdatasync() operation");
			kill(getpid(), 2);
		}
	}
}

void *clientThread(void *thread_param){

	sigset_t sigset;
	sigfillset(&sigset);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	thread_client* thread_args = (thread_client *) thread_param;
	thread_args->thread_complete=false;
	thread_args->tid=pthread_self();

	char msg[25]="\nSend key :\n";
	char key[5];

	while(!srv_terminated){

		int sendLen=send(thread_args->nsfd, msg, strlen(msg), 0);
		if(sendLen==-1){
			syslog(LOG_DEBUG,"send() packets failed\n");
			thread_args->thread_complete=true;
			if(pthread_mutex_unlock(thread_args->mutex))
				syslog(LOG_DEBUG,"pthread_mutex_unlock() failed\n");
			break;
		}

		int recvLen=recv(thread_args->nsfd, key, 5, 0);
		if(recvLen==0||recvLen==-1){
			syslog(LOG_DEBUG,"Closed connection from %s\n", thread_args->cln_addr);
			thread_args->thread_complete=true;
			break;
		}

		if(strncmp(key,"123\n",4)==0){

			if(pthread_mutex_lock(thread_args->mutex)==0){

				thread_args->fd=open(FILE_PATH,O_RDWR);
				if(thread_args->fd==-1){
					syslog(LOG_DEBUG,"fopen() aesdchar failed\n");
					thread_args->thread_complete=true;
					if(pthread_mutex_unlock(thread_args->mutex))
						syslog(LOG_DEBUG,"pthread_mutex_unlock() failed\n");
					break;
				}

				struct aesd_seekto seekto;
				seekto.write_cmd=0;
				seekto.write_cmd_offset=0;

				if(ioctl(thread_args->fd, AESDCHAR_IOCSEEKTO, &seekto)){
					syslog(LOG_DEBUG,"ioctl() failed\n");
					close(thread_args->fd);
					thread_args->thread_complete=true;
					if(pthread_mutex_unlock(thread_args->mutex))
						syslog(LOG_DEBUG,"pthread_mutex_unlock() failed\n");
					break;
				}

				char buf[MEM_SIZE];
				int readLen=0,sendLen=0;
				while((readLen=read(thread_args->fd,buf,MEM_SIZE))){
					sendLen=send(thread_args->nsfd, buf, readLen, 0);
					if(sendLen==-1){
						syslog(LOG_DEBUG,"send() packets failed\n");
						close(thread_args->fd);
						thread_args->thread_complete=true;
						if(pthread_mutex_unlock(thread_args->mutex))
							syslog(LOG_DEBUG,"pthread_mutex_unlock() failed\n");
						break;
					}
				}
				
				close(thread_args->fd);
				if(pthread_mutex_unlock(thread_args->mutex))
					syslog(LOG_DEBUG,"pthread_mutex_unlock() failed\n");

			}
			else{
				syslog(LOG_DEBUG,"pthread_mutex_lock() failed\n");
				break;
			}
		}
		else{
			char warn[25]="Key mismatch, try again\n";
			sendLen=send(thread_args->nsfd, warn, strlen(warn), 0);
			if(sendLen==-1){
				syslog(LOG_DEBUG,"send() packets failed\n");
				thread_args->thread_complete=true;
				if(pthread_mutex_unlock(thread_args->mutex))
					syslog(LOG_DEBUG,"pthread_mutex_unlock() failed\n");
				break;
			}
		}
	}

	shutdown(thread_args->nsfd,SHUT_RDWR);
	close(thread_args->nsfd);

	thread_args->thread_complete=true;
	pthread_exit(thread_args);
}

void strcatString(char *s1, char *s2){

	if(strcat(s1, s2)==NULL){
		syslog(LOG_ERR,"strcat()");
		kill(getpid(), 2);
	}
}

void setValue(int fd1, int fd2, char *val1, char *val2){

	fileReadWrite(fd1, val1, 1, 1);
	fileReadWrite(fd2, val2, 1, 1);
}

void toggle(int fd){

	char buf[1];
	fileReadWrite(fd, buf, 1, 0);

	if(buf[0]=='0')
		buf[0]='1';
	else if(buf[0]=='1')
		buf[0]='0';

	fileReadWrite(fd, buf, 1, 1);
}

void *sw(void *thread_param){

	sigset_t sigset;
	sigfillset(&sigset);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	thread_switch *thread_args = (thread_switch*) thread_param;
	thread_args->tid=pthread_self();

	int totalCount=0, forwardCount=0, reverseCount=0, stopCount=0, readVal=1;
	char buf[1], status[3][20]={"Forward","Reverse","Stopped"};

	while(!srv_terminated){
		sleep(1);
		if(readVal==1){
			fileReadWrite(thread_args->fd_swVal, buf, 1, 0);
			if(buf[0]=='0'){
				readVal=0;
			}
		}
		if(readVal==0){
			readVal=1;
			char *currentStatus=status[2];
			int option=totalCount%4;
			toggle(thread_args->fd_ledVal);
			switch(option){
				case 0:
					setValue(thread_args->fd_motorVal1, thread_args->fd_motorVal2, "0", "1");
					currentStatus=status[0];
					++forwardCount;
					break;
				case 2:
					setValue(thread_args->fd_motorVal1, thread_args->fd_motorVal2, "1", "0");
					currentStatus=status[1];
					++reverseCount;
					break;
				default:
					setValue(thread_args->fd_motorVal1, thread_args->fd_motorVal2, "0", "0");
					++stopCount;
					break;
			}
			++totalCount;

			if(pthread_mutex_lock(thread_args->mutex)==0){

				thread_args->fd=open(FILE_PATH,O_RDWR);
				if(thread_args->fd==-1){
					syslog(LOG_ERR,"fopen() driver\n");
					if(pthread_mutex_unlock(thread_args->mutex))
						syslog(LOG_ERR,"pthread_mutex_unlock()\n");
					kill(getpid(), 2);
					break;
				}

				char str1[256];
				char str2[128];

				sprintf(str1,"Motor Num=%d Status=%s ---> ",thread_args->num, currentStatus);
				sprintf(str2,"Forward count=%d, Reverse count=%d, Stop count=%d\n",forwardCount, reverseCount, stopCount);

				strcatString(str1, str2);
				int writeLen=write(thread_args->fd, str1, strlen(str1));        
				if(writeLen==-1){
					syslog(LOG_ERR,"write() driver\n");
					close(thread_args->fd);
					if(pthread_mutex_unlock(thread_args->mutex))
						syslog(LOG_ERR,"pthread_mutex_unlock()\n");
					kill(getpid(), 2);
					break;
				}

				close(thread_args->fd);

				if(pthread_mutex_unlock(thread_args->mutex)){
					syslog(LOG_DEBUG,"pthread_mutex_unlock() failed\n");
					kill(getpid(), 2);
					break;
				}

			}
			else{
				syslog(LOG_DEBUG,"pthread_mutex_lock() failed\n");
				kill(getpid(), 2);
				break;
			}
		}
	}

        pthread_exit(thread_args);

}

void setDirection(char *gpioNum, char *dirVal){

	char *dirPath="/direction";
	char gpio[10]="gpio";
	char gpioPath[40]="/sys/class/gpio/";
	char cmd[20]="gpioExport.sh ";

	strcatString(cmd, gpioNum);
	strcatString(gpio, gpioNum);
	strcatString(gpioPath, gpio);
	strcatString(gpioPath, dirPath);

	if(system(cmd)){
		syslog(LOG_ERR,"system() export");
		cleanup();
		exit(1);
	}

	fd_dir=open(gpioPath, O_WRONLY|O_TRUNC);
	if(fd_dir==-1){
		syslog(LOG_ERR,"open() gpio direction");
		cleanup();
		exit(1);
	}

	fileReadWrite(fd_dir, dirVal, strlen(dirVal), 1);

	close(fd_dir);
	fd_dir=-1;
}

void setFile(char *gpioNum, int mode, int *fd){

	char *valPath="/value";
	char gpio[10]="gpio";
	char gpioPath[40]="/sys/class/gpio/";

	strcatString(gpio, gpioNum);
	strcatString(gpioPath, gpio);
	strcatString(gpioPath, valPath);

	*fd=open(gpioPath, mode);
	if(*fd==-1){
		syslog(LOG_ERR,"open() gpio value");
		cleanup();
		exit(1);
	}

	if(mode!=O_RDONLY){
		fileReadWrite(*fd, "0", 1, 1);
	}

}

void socketServer(){

	struct sockaddr_in srv,cln;
	sfd=socket(AF_INET,SOCK_STREAM,0);
	if(sfd==-1){
		syslog(LOG_ERR,"socket() failed\n");
		closelog();
		exit(1);
	}

	syslog(LOG_DEBUG,"Socket created....\n");
	srv.sin_family=AF_INET;
	srv.sin_port=htons(PORT);
	srv.sin_addr.s_addr=inet_addr("0.0.0.0");

	int yes=1;
	if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &yes, sizeof(int)) == -1) {
		syslog(LOG_ERR,"setsockopt() failed\n");
		cleanup();
		exit(1);
	}

	if(bind(sfd,(struct sockaddr *)&srv, sizeof(srv))){
		syslog(LOG_ERR,"bind() failed\n");
		cleanup();
		exit(1);
	}

	if(listen(sfd,5)){
		syslog(LOG_ERR,"listen() failed\n");
		cleanup();
		exit(1);
	}

	while(1){

		unsigned int len=sizeof(cln);
		syslog(LOG_DEBUG,"Waiting for client....\n");
		int nsfd=accept(sfd,(struct sockaddr*)&cln, &len);
		if(nsfd==-1){
			syslog(LOG_ERR,"accept() failed\n");
			cleanup();
			exit(1);
		}
		syslog(LOG_DEBUG,"Accepted connection from %s\n", inet_ntoa(cln.sin_addr));

		struct entry *data=(struct entry*)malloc(sizeof(struct entry));

		if(data!=NULL){

			data->thread_client_param.mutex=&mutex;
			data->thread_client_param.fd=fd;
			data->thread_client_param.nsfd=nsfd;
			strcpy(data->thread_client_param.cln_addr, inet_ntoa(cln.sin_addr));
			data->thread_client_param.thread_complete=false;

			if(pthread_create(&(data->thread_client_param.tid), NULL, clientThread, &(data->thread_client_param))){
				syslog(LOG_ERR,"pthread_create() client failed\n");
				cleanup();
				exit(1);
			}

			SLIST_INSERT_HEAD(&head, data, entries);

		}
		else{
			syslog(LOG_ERR,"malloc() failed for clent thread param\n");
			cleanup();
			exit(1);
		}

		struct entry *checkData=NULL, *checkList=NULL;
		checkData = SLIST_FIRST(&head);
		SLIST_FOREACH_SAFE(checkData, &head, entries, checkList){
			if(checkData->thread_client_param.thread_complete){
				pthread_join(checkData->thread_client_param.tid,NULL);
				SLIST_REMOVE(&head, checkData, entry, entries);
				free(checkData);
			}
		}
	}
}

void startSwitchThread(){

	char dir[2][5]={"in","out"};
	char pin[8][5]={"27","65","30","60","44","45","46","47"};

	setDirection(pin[0], dir[1]);
	setDirection(pin[1], dir[1]);
	setDirection(pin[2], dir[1]);
	setDirection(pin[3], dir[1]);
	setDirection(pin[4], dir[1]);
	setDirection(pin[5], dir[1]);
	setDirection(pin[6], dir[0]);
	setDirection(pin[7], dir[0]);

	sw1_param = (thread_switch*) malloc(sizeof(thread_switch));
	if(sw1_param==NULL){
		syslog(LOG_ERR,"malloc() sw1\n");
		cleanup();
		exit(1);
	}
	sw1_param->num=1;
	sw1_param->mutex=&mutex;
	sw1_param->tid=-1;
	sw1_param->fd_swVal=-1;
	sw1_param->fd_ledVal=-1;
	sw1_param->fd_motorVal1=-1;
	sw1_param->fd_motorVal2=-1;

	sw2_param = (thread_switch*) malloc(sizeof(thread_switch));
	if(sw2_param==NULL){
		syslog(LOG_ERR,"malloc() sw2\n");
		cleanup();
		exit(1);
	}
	sw2_param->num=2;
	sw2_param->mutex=&mutex;
	sw2_param->tid=-1;
	sw2_param->fd_swVal=-1;
	sw2_param->fd_ledVal=-1;
	sw2_param->fd_motorVal1=-1;
	sw2_param->fd_motorVal2=-1;

	setFile(pin[0], O_RDWR, &(sw1_param->fd_motorVal1));
	setFile(pin[1], O_RDWR, &(sw1_param->fd_motorVal2));
	setFile(pin[2], O_RDWR, &(sw2_param->fd_motorVal1));
	setFile(pin[3], O_RDWR, &(sw2_param->fd_motorVal2));

	setFile(pin[4], O_RDWR, &(sw1_param->fd_ledVal));
	setFile(pin[5], O_RDWR, &(sw2_param->fd_ledVal));
	setFile(pin[6], O_RDONLY, &(sw1_param->fd_swVal));
	setFile(pin[7], O_RDONLY, &(sw2_param->fd_swVal));

	if(pthread_create(&(sw1_param->tid), NULL, sw, sw1_param)){
		syslog(LOG_ERR,"pthread_create() failed for sw1\n");
		cleanup();
		exit(1);
	}

	if(pthread_create(&(sw2_param->tid), NULL, sw, sw2_param)){
		syslog(LOG_ERR,"pthread_create() failed for sw2\n");
		cleanup();
		exit(1);
	}

}

int main( int argc, char *argv[]){

	openlog("socket_app", 0, LOG_USER);

	struct sigaction s;
	s.sa_handler=signalHandler;
	sigemptyset(&s.sa_mask);
	s.sa_flags=0;
	sigaction(2,&s,0);
	sigaction(15,&s,0);

	sigset_t sigset;
	sigfillset(&sigset);
	sigdelset(&sigset,2);
	sigdelset(&sigset,15);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	char *option="-d";
	int enableDaemon=0;

	if(argc>2){
		syslog(LOG_ERR,"More arguments than expected\n");
		closelog();
		exit(1);
	}

	if(argc==2){

		if(strcmp(option,argv[1])==0){
			enableDaemon=1;
			syslog(LOG_DEBUG,"Running as daemon\n");
		}
		else{
			syslog(LOG_ERR,"Invalid argument use -d\n");
			closelog();
			exit(1);
		}
	}

	if(enableDaemon){

		pid_t pid=0;
		pid_t sid=0;

		pid=fork();
		if(pid<0){
			syslog(LOG_ERR,"fork() failed\n");
			exit(1);
		}
		if(pid>0){
			exit(0); //exit the parent
		}

		umask(0);
		sid=setsid();

		if(sid<0){
			syslog(LOG_ERR,"setsid() failed\n");
			exit(1);
		}

		int ret=chdir("/");
		if(ret<0){
			syslog(LOG_ERR,"chdir() failed\n");
			exit(1);
		}

		close(0);
		close(1);
		close(2);
		int fdNull=open("/dev/null", O_RDWR, 0666);
		if(fdNull<0){
			syslog(LOG_ERR,"open() /dev/null failed\n");
			exit(1);
		}
		dup(0);
		dup(0);
		syslog(LOG_DEBUG,"Daemon PID=%d\n",getpid());

	}

	SLIST_INIT(&head);

	syslog(LOG_DEBUG,"Starting Motor-Server app\n");
	startSwitchThread();
	socketServer();

}

