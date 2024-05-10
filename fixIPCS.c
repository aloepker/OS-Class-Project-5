#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>
#include <errno.h>

#define SHMKEY 859048
#define BUFF_SZ sizeof (int)

#define PERMS 0777
typedef struct msgbuffer {
	long mtype;
	char strData[100];
	int intData;
} msgbuffer;

int main(int argc, char** argv){
	//initializing shared memory
	int shmid = shmget ( SHMKEY, BUFF_SZ, 0777 | IPC_CREAT );
	if ( shmid == -1 ){
		perror("Shared Memory Creation Error!!!\n");
		return EXIT_FAILURE;
	}
	char * paddr = ( char * )( shmat ( shmid, 0, 0 ) );
	int * shmTime = ( int * )( paddr );
	shmTime[0]=123;
	shmTime[1]=123;
	 //shmdt(shmTime);
	//message que initial implementation:
	msgbuffer buf0, buf1;
	int msqid;
	key_t key;
	system("touch msgq.txt");
	//set message queue key
	if ((key = ftok("msgq.txt", 1)) == -1) {
		perror("parent ftok error");
		exit(1);
	}
	//creates message queue
	if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
		perror("msgget in parent error");
		exit(1);
	}
		//close shared memory and output file:
		shmdt(shmTime);//was ppint
		shmctl(shmid, IPC_RMID, NULL);
		printf("OSS: shared memory shutdown successful\n");
		//clear message ques:
		if (msgctl(msqid, IPC_RMID, NULL) == -1){
			perror("msgctl failed to get rid of que in parent ");
			exit(1);
		}
}
