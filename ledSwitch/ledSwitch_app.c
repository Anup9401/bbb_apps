
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
}thread_switch;

int fd44_dir=-1, fd45_dir=-1;
int fd46_dir=-1, fd47_dir=-1;
thread_switch *sw1_param=NULL, *sw2_param=NULL;

void cleanup(){

	if(fd44_dir!=-1){
		close(fd44_dir);
	}
	if(fd45_dir!=-1){
		close(fd45_dir);
	}
	if(fd46_dir!=-1){
		close(fd46_dir);
	}
	if(fd47_dir!=-1){
		close(fd47_dir);
	}
	if(sw1_param!=NULL){
		if(sw1_param->tid!=-1){
			if(pthread_cancel(sw1_param->tid)){
				perror("pthread_cancel() sw1 failed\n");
			}
		}
		if(sw1_param->fd_swVal!=-1){
			close(sw1_param->fd_swVal);
		}
		if(sw1_param->fd_ledVal!=-1){
			close(sw1_param->fd_ledVal);
		}
		free(sw1_param);
	}
	if(sw2_param!=NULL){
		if(sw2_param->tid!=-1){
			if(pthread_cancel(sw2_param->tid)){
				perror("pthread_cancel() sw2 failed\n");
			}
		}
		if(sw2_param->fd_swVal!=-1){
			close(sw2_param->fd_swVal);
		}
		if(sw2_param->fd_ledVal!=-1){
			close(sw2_param->fd_ledVal);
		}
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

void toggle(int fd){

	char buf[1];
	lseek(fd,0,SEEK_SET);
	if(read(fd, buf, 1)==-1){
		perror("read() toggle");
		raise(2);
	}
	lseek(fd,0,SEEK_SET);

	if(buf[0]=='0')
		buf[0]='1';
	else if(buf[0]=='1')
		buf[0]='0';

	if(write(fd, buf, 1)==-1){
		perror("write() toggle");
		raise(2);
	}

	if(fdatasync(fd)==-1){
		perror("fdatasync() toggle");
		raise(2);
	}
}

void *sw(void *thread_param){

	sigset_t sigset;
	sigfillset(&sigset);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	thread_switch *thread_args = (thread_switch*) thread_param;
	thread_args->tid=pthread_self();

	int count=0, readVal=1;
	char buf[1];

	while(1){
		sleep(1);
		if(readVal==1){
			lseek(thread_args->fd_swVal,0,SEEK_SET);
			if(read(thread_args->fd_swVal, buf, 1)==-1){
				perror("read() switch");
				raise(2);
			}
			if(buf[0]=='0')
				readVal=0;
		}
		if(readVal==0){
			readVal=1;
			toggle(thread_args->fd_ledVal);	
			printf("Button pressed for pin=%d count=%d\n",thread_args->num,++count);
		}

	}

}

void setPin(char *gpioNum, char *dirVal, int *fd){

	char *dirPath="/direction";
	char gpio[10]="gpio";
	char gpioPath[40]="/sys/class/gpio/";
	char cmd[20]="gpioExport.sh ";
	
	if(strcat(cmd,gpioNum)==NULL){
		perror("strcat() cmd");
		cleanup();
		exit(1);
	}
	if(strcat(gpio,gpioNum)==NULL){
		perror("strcat() gpio");
		cleanup();
		exit(1);
	}
	if(strcat(gpioPath,gpio)==NULL){
		perror("strcat() gpioNum");
		cleanup();
		exit(1);
	}
	if(strcat(gpioPath,dirPath)==NULL){
		perror("strcat() gpioPath");
		cleanup();
		exit(1);
	}

	if(system(cmd)){
		perror("system() export gpio");
		cleanup();
		exit(1);
	}

	*fd=open(gpioPath, O_WRONLY|O_TRUNC);
	if(*fd==-1){
		perror("open() gpio direction");
		cleanup();
		exit(1);
	}
	if(write(*fd, dirVal, strlen(dirVal))==-1){
		perror("write() gpio direction");
		cleanup();
		exit(1);
	}
	if(fdatasync(*fd)==-1){
		perror("fdatasync() gpio direction");
		cleanup();
		exit(1);
	}

	close(*fd);
	*fd=-1;

}

void setFile(char *gpioNum, int mode, int *fd){

	char *valPath="/value";
	char gpio[10]="gpio";
	char gpioPath[40]="/sys/class/gpio/";

	if(strcat(gpio,gpioNum)==NULL){
		perror("strcat() gpio file");
		cleanup();
		exit(1);
	}
	if(strcat(gpioPath,gpio)==NULL){
		perror("strcat() gpioNum file");
		cleanup();
		exit(1);
	}
	if(strcat(gpioPath,valPath)==NULL){
		perror("strcat() gpioPath file");
		cleanup();
		exit(1);
	}

	*fd=open(gpioPath, mode);
	if(*fd==-1){
		perror("open() gpio val");
		cleanup();
		exit(1);
	}

	if(mode!=O_RDONLY){
		
		lseek(*fd,0,SEEK_SET);
		if(write(*fd, "0", 1)==-1){
			perror("write() gpio value");
			cleanup();
			exit(1);
		}
		if(fdatasync(*fd)==-1){
			perror("fdatasync() gpio value");
			cleanup();
			exit(1);
		}
	}

}

int main(int argc, char *argv[]){

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

	char dir[2][5]={"in","out"};
	char pin[4][5]={"44","45","46","47"};
	setPin(pin[0],dir[1],&fd44_dir);
	setPin(pin[1],dir[1],&fd45_dir);
	setPin(pin[2],dir[0],&fd46_dir);
	setPin(pin[3],dir[0],&fd47_dir);

	sw1_param = (thread_switch*) malloc(sizeof(thread_switch));
	if(sw1_param==NULL){
		perror("malloc() sw1\n");
		cleanup();
		exit(1);
	}
	sw1_param->num=16;
	sw1_param->mutex=&mutex;
	sw1_param->tid=-1;
	sw1_param->fd_swVal=-1;
	sw1_param->fd_ledVal=-1;

	sw2_param = (thread_switch*) malloc(sizeof(thread_switch));
	if(sw2_param==NULL){
		perror("malloc() sw2\n");
		cleanup();
		exit(1);
	}
	sw2_param->num=15;
	sw2_param->mutex=&mutex;
	sw2_param->tid=-1;
	sw2_param->fd_swVal=-1;
	sw2_param->fd_ledVal=-1;

	setFile(pin[0], O_RDWR, &(sw1_param->fd_ledVal));
	setFile(pin[1], O_RDWR, &(sw2_param->fd_ledVal));
	setFile(pin[2], O_RDONLY, &(sw1_param->fd_swVal));
	setFile(pin[3], O_RDONLY, &(sw2_param->fd_swVal));

	if(pthread_create(&(sw1_param->tid), NULL, sw, sw1_param)){
		perror("pthread_create() failed for sw1\n");
		cleanup();
		exit(1);
	}

	if(pthread_create(&(sw2_param->tid), NULL, sw, sw2_param)){
		perror("pthread_create() failed for sw2\n");
		cleanup();
		exit(1);
	}

	printf("Waiting for user input at pin 15 and pin 16 of P8 block of BBB\n");
	while(1);
}

