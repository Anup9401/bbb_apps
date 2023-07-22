
#include<stdio.h>
#include<stdlib.h>
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
	int fd_val;
}thread_switch;

int fd46_dir=-1, fd47_dir=-1;
thread_switch *sw1_param=NULL, *sw2_param=NULL;

void cleanup(){
	
	if(fd46_dir!=-1){
		close(fd46_dir);
		fd46_dir=-1;
	}
	if(fd47_dir!=-1){
		close(fd47_dir);
		fd47_dir=-1;
	}
	if(sw1_param!=NULL){
		if(sw1_param->tid!=-1){
			if(pthread_cancel(sw1_param->tid)){
				perror("pthread_cancel() sw1 failed\n");
			}
		}
		if(sw1_param->fd_val!=-1){
			close(sw1_param->fd_val);
		}
		free(sw1_param);
	}
	if(sw2_param!=NULL){
		if(sw2_param->tid!=-1){
			if(pthread_cancel(sw2_param->tid)){
				perror("pthread_cancel() sw2 failed\n");
			}
		}
		if(sw2_param->fd_val!=-1){
			close(sw2_param->fd_val);
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
			lseek(thread_args->fd_val,0,SEEK_SET);
			if(read(thread_args->fd_val, buf, 1)==-1){
				perror("read() switch");
				raise(2);
			}
			if(buf[0]=='0')
				readVal=0;
		}
		if(readVal==0){
			readVal=1;	
			printf("Button pressed for pin=%d count=%d\n",thread_args->num,++count);
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

	if(system("gpioExport.sh 46")){
		perror("system() export gpio46");
		exit(1);
	}
	if(system("gpioExport.sh 47")){
		perror("system() export gpio47");
		exit(1);
	}

	fd46_dir=open("/sys/class/gpio/gpio46/direction",O_WRONLY|O_TRUNC);
	if(fd46_dir==-1){
		perror("open() gpio46 direction");
		cleanup();
		exit(1);
	}
	fd47_dir=open("/sys/class/gpio/gpio47/direction",O_WRONLY|O_TRUNC);
	if(fd47_dir==-1){
		perror("open() gpio47 direction");
		cleanup();
		exit(1);
	}

	if(write(fd46_dir, "in", 2)==-1){
		perror("write() gpio46 direction");
		cleanup();
		exit(1);
	}

	if(write(fd47_dir, "in", 2)==-1){
		perror("write() gpio47 direction");
		cleanup();
		exit(1);
	}

	if(fdatasync(fd46_dir)==-1){
		perror("fdatasync() gpio46 direction");
		cleanup();
		exit(1);
	}
	if(fdatasync(fd47_dir)==-1){
		perror("fdatasync() gpio47 direction");
		cleanup();
		exit(1);
	}

	close(fd46_dir);
	close(fd47_dir);
	fd46_dir=-1;
	fd47_dir=-1;

	sw1_param = (thread_switch*) malloc(sizeof(thread_switch));
	if(sw1_param==NULL){
		perror("malloc() sw1\n");
		cleanup();
		exit(1);
	}
	sw1_param->num=16;
	sw1_param->mutex=&mutex;
	sw1_param->tid=-1;
	sw1_param->fd_val=-1;

	sw2_param = (thread_switch*) malloc(sizeof(thread_switch));
	if(sw2_param==NULL){
		perror("malloc() sw2\n");
		cleanup();
		exit(1);
	}
	sw2_param->num=15;
	sw2_param->mutex=&mutex;
	sw2_param->tid=-1;
	sw2_param->fd_val=-1;

	sw1_param->fd_val=open("/sys/class/gpio/gpio46/value",O_RDONLY);
	if(sw1_param->fd_val==-1){
		perror("open() gpio46 value");
		cleanup();
		exit(1);
	}
	sw2_param->fd_val=open("/sys/class/gpio/gpio47/value",O_RDONLY);
	if(sw2_param->fd_val==-1){
		perror("open() gpio47 value");
		cleanup();
		exit(1);
	}

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

