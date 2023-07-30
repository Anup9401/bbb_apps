
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<fcntl.h>
#include<signal.h>
#include<pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
	int num;
	pthread_mutex_t *mutex;
	pthread_t tid;
	int fd_swVal;
	int fd_ledVal;
	int fd_motorVal1;
	int fd_motorVal2;
}thread_switch;

int fd_dir=-1;
thread_switch *sw1_param=NULL, *sw2_param=NULL;

void fileReadWrite(int fd, char *val, int len, int op){

	lseek(fd,0,SEEK_SET);
	if(op==0){
		if(read(fd, val, len)==-1){
			perror("read() operation");
			raise(2);
		}

	}
	else if(op==1){
		if(write(fd, val, len)==-1){
			perror("write() operation");
			raise(2);
		}
		if(fdatasync(fd)==-1){
			perror("fdatasync() operation");
			raise(2);
		}
	}
}

void closeFile(int fd, int clear){

	if(fd!=-1){
		if(clear){
			fileReadWrite(fd, "0", 1, 1);
		}
		close(fd);
	}
}

void cleanup(){

	closeFile(fd_dir,0);
	if(sw1_param!=NULL){
		if(sw1_param->tid!=-1){
			if(pthread_cancel(sw1_param->tid)){
				perror("pthread_cancel() sw1 failed\n");
			}
		}
		closeFile(sw1_param->fd_swVal,0);
		closeFile(sw1_param->fd_ledVal,1);
		closeFile(sw1_param->fd_motorVal1,1);
		closeFile(sw1_param->fd_motorVal2,1);
		free(sw1_param);
	}
	if(sw2_param!=NULL){
		if(sw2_param->tid!=-1){
			if(pthread_cancel(sw2_param->tid)){
				perror("pthread_cancel() sw2 failed\n");
			}
		}
		closeFile(sw2_param->fd_swVal,0);
		closeFile(sw2_param->fd_ledVal,1);
		closeFile(sw2_param->fd_motorVal1,1);
		closeFile(sw2_param->fd_motorVal2,1);
		free(sw2_param);
	}
	if(pthread_mutex_destroy(&mutex)){
		perror("pthread_mutex_destroy() failed\n");
	}
}

void signalHandler(int signal){

	if(signal==2||signal==15){
		cleanup();
		printf("caught signal, exiting\n");
		exit(0);
	}
}

void strcatString(char *s1, char *s2){

	if(strcat(s1, s2)==NULL){
		perror("strcat()");
		raise(2);
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

	while(1){
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
			printf("Motor Num=%d Status=%s ---> ",thread_args->num, currentStatus);
			printf("Forward count=%d, Reverse count=%d, Stop count=%d\n",forwardCount, reverseCount, stopCount);
		}

	}

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
		perror("system() export");
		raise(2);
	}

	fd_dir=open(gpioPath, O_WRONLY|O_TRUNC);
	if(fd_dir==-1){
		perror("open() gpio direction");
		raise(2);
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
		perror("open() gpio value");
		cleanup();
		exit(1);
	}

	if(mode!=O_RDONLY){
		fileReadWrite(*fd, "0", 1, 1);
	}

}

int main(int argc, char *argv[]){

	char dir[2][5]={"in","out"};
	char pin[8][5]={"27","65","30","60","44","45","46","47"};

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
		perror("malloc() sw1\n");
		raise(2);
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
		perror("malloc() sw2\n");
		raise(2);
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
		perror("pthread_create() failed for sw1\n");
		raise(2);
	}

	if(pthread_create(&(sw2_param->tid), NULL, sw, sw2_param)){
		perror("pthread_create() failed for sw2\n");
		raise(2);
	}

	printf("Starting Motor App\n");
	while(1);

}

