
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<fcntl.h>
#include<signal.h>

int fd_dir;
int fd27_val=-1, fd65_val=-1; //BBB pin 17 and 18 on P8
int fd30_val=-1, fd60_val=-1; //BBB pin 11 and 12 on P9

void closeFile(int fd){

	if(fd!=-1){
		close(fd);
	}
}

void cleanup(){

	closeFile(fd_dir);
	closeFile(fd27_val);
	closeFile(fd65_val);
	closeFile(fd30_val);
	closeFile(fd60_val);
}

void signalHandler(int signal){

	if(signal==2||signal==15){
		cleanup();
		printf("caught signal, exiting\n");
		exit(0);
	}
}

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

	int option;
	char dir[2][5]={"in","out"};
	char pin[4][5]={"27","65","30","60"};

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

	setFile(pin[0], O_RDWR, &fd27_val);
	setFile(pin[1], O_RDWR, &fd65_val);
	setFile(pin[2], O_RDWR, &fd30_val);
	setFile(pin[3], O_RDWR, &fd60_val);

	while(1){

		printf("Enter the option\n");
		printf("0.QUIT\n");
		printf("1.Run motor-1 alone in forward direction\n");
		printf("2.Run motor-2 alone in forward direction\n");
		printf("3.Run motor-1 alone in reverse direction\n");
		printf("4.Run motor-2 alone in reverse direction\n");
		printf("5.Run motor-1 and motor-2 in forward direction\n");
		printf("6.Run motor-1 and motor-2 in reverse direction\n");
		printf("7.Stop motor-1\n");
		printf("8.Stop motor-2\n");
		printf("9.Stop both the motors\n");
		scanf("%d", &option);

		switch(option){
			case 0 :
				raise(2);
				break;
			case 1 : 
				setValue(fd27_val, fd65_val, "0", "1");
				setValue(fd30_val, fd60_val, "0", "0");
				break;
			case 2 : 
				setValue(fd27_val, fd65_val, "0", "0");
				setValue(fd30_val, fd60_val, "0", "1");
				break;
			case 3 : 
				setValue(fd27_val, fd65_val, "1", "0");
				setValue(fd30_val, fd60_val, "0", "0");
				break;
			case 4 : 
				setValue(fd27_val, fd65_val, "0", "0");
				setValue(fd30_val, fd60_val, "1", "0");
				break;
			case 5 : 
				setValue(fd27_val, fd65_val, "0", "1");
				setValue(fd30_val, fd60_val, "0", "1");
				break;
			case 6 : 
				setValue(fd27_val, fd65_val, "1", "0");
				setValue(fd30_val, fd60_val, "1", "0");
				break;
			case 7 : 
				setValue(fd27_val, fd65_val, "0", "0");
				break;
			case 8 : 
				setValue(fd30_val, fd60_val, "0", "0");
				break;
			case 9 : 
				setValue(fd27_val, fd65_val, "0", "0");
				setValue(fd30_val, fd60_val, "0", "0");
				break;
			default:
				printf("Wrong option...Try again...\n");
				break;
		}

	}
}

