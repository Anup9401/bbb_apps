
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<unistd.h>
#include<fcntl.h>

int fd30_dir=-1, fd60_dir=-1;
int fd30_val=-1, fd60_val=-1;

void cleanup(){
	if(fd30_dir!=-1){
		close(fd30_dir);
		fd30_dir=-1;
	}
	if(fd60_dir!=-1){
		close(fd60_dir);
		fd60_dir=-1;
	}
	if(fd30_val!=-1){
		close(fd30_val);
		fd30_val=-1;
	}
	if(fd60_val!=-1){
		close(fd60_val);
		fd60_val=-1;
	}
}

void toggle(int fd){

	char buf[1];
	lseek(fd,0,SEEK_SET);
	if(read(fd, buf, 1)==-1){
		perror("read() toggle");
		cleanup();
		exit(1);
	}
	lseek(fd,0,SEEK_SET);

	if(buf[0]=='0')
		buf[0]='1';
	else if(buf[0]=='1')
		buf[0]='0';

	if(write(fd, buf, 1)==-1){
		perror("write() toggle");
		cleanup();
		exit(1);
	}

	if(fdatasync(fd)==-1){
		perror("fdatasync() toggle");
		cleanup();
		exit(1);
	}
}

int main(){

	int opt;
	if(system("gpioExport.sh 30")){
		perror("system() export gpio30");
		exit(1);
	}
	if(system("gpioExport.sh 60")){
		perror("system() export gpio60");
		exit(1);
	}

	fd30_dir=open("/sys/class/gpio/gpio30/direction",O_WRONLY|O_TRUNC);
	if(fd30_dir==-1){
		perror("open() gpio30 direction");
		cleanup();
		exit(1);
	}
	fd60_dir=open("/sys/class/gpio/gpio60/direction",O_WRONLY|O_TRUNC);
	if(fd60_dir==-1){
		perror("open() gpio60 direction");
		cleanup();
		exit(1);
	}

	if(write(fd30_dir, "out", 3)==-1){
		perror("write() gpio30 direction");
		cleanup();
		exit(1);
	}

	if(write(fd60_dir, "out", 3)==-1){
		perror("write() gpio60 direction");
		cleanup();
		exit(1);
	}

	if(fdatasync(fd30_dir)==-1){
		perror("fdatasync() gpio30 direction");
		cleanup();
		exit(1);
	}
	if(fdatasync(fd60_dir)==-1){
		perror("fdatasync() gpio60 direction");
		cleanup();
		exit(1);
	}

	close(fd30_dir);
	close(fd60_dir);
	fd30_dir=-1;
	fd60_dir=-1;

	fd30_val=open("/sys/class/gpio/gpio30/value",O_RDWR);
	if(fd30_val==-1){
		perror("open() gpio30 value");
		cleanup();
		exit(1);
	}
	fd60_val=open("/sys/class/gpio/gpio60/value",O_RDWR);
	if(fd60_val==-1){
		perror("open() gpio60 value");
		cleanup();
		exit(1);
	}

	if(write(fd30_val, "0", 1)==-1){
		perror("write() gpio30 value");
		cleanup();
		exit(1);
	}

	if(fdatasync(fd30_val)==-1){
		perror("fdatasync() gpio30 value");
		cleanup();
		exit(1);
	}

	if(write(fd60_val, "0", 1)==-1){
		perror("write() gpio60 value");
		cleanup();
		exit(1);
	}

	if(fdatasync(fd60_val)==-1){
		perror("fdatasync() gpio60 value");
		cleanup();
		exit(1);
	}

	while(1){

		printf("Enter an option : \n");
		printf("1-Toggle LED at BBB pin 11\n");
		printf("2-Toggle LED at BBB pin 12\n");
		printf("3-QUIT\n");
		scanf("%d", &opt);

		if(opt==1){
			toggle(fd30_val);
		}
		else if(opt==2){
			toggle(fd60_val);
		}
		else if(opt==3)
			break;
		else
			printf("Invalid option try again...\n");

	}

	cleanup();
	exit(0);

}


