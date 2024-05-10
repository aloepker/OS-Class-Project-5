//Written by Adam Spencer Loepker
//Finished on 5/10/2024
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
#include <math.h>
#include <float.h>
//Simulated clock functions and variables:
const int schTime = 50000000; //50ns is 50mil ms
int sysClockNano = 0;
int sysClockSec = 0;
void incrementClock(){
	sysClockNano = sysClockNano + 1000000;
	if (sysClockNano > 1000000000){
		sysClockSec++;
		sysClockNano -= 1000000000;
	}
}
void incrementByX(int x){
	sysClockNano = sysClockNano + x;
	if (sysClockNano > 1000000000){
		sysClockSec++;
		sysClockNano -= 1000000000;
	}
}

int randSeconds(int max){
	return rand()%max;
}
int randNano(){
	return 1+rand()%1000000000;
}
//Process Control Block:
struct PCB {
	int occupied;
	pid_t pid;
	int startSeconds; //birth time seconds
	int startNano; //birth time nano
	int serviceTimeSec; //total seconds process has been running
	int serviceTimeNano; //total nanoseconds process ha been running
	int eventWaitSec; //seconds before unblock
	int eventWaitNano; //nano before unblock
	int blocked; //is the process blocked?
};
struct PCB processTable[20];
//Display process control block:
void printPCB(int smS, int smN, FILE *file){
	printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), smS, smN);
	fprintf(file, "OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), smS, smN);
	printf("Process Table:\n");
	fprintf(file, "Process Table:\n");
	printf("Entry Occupied PID     StartS    StartN    SecActive   NanoActive   SecIOw   NanoIOw Blocked\n");
	fprintf(file, "Entry Occupied PID     StartS    StartN    SecActive   NanoActive   SecIOw   NanoIOw Blocked\n");
	int m;
	for (m=0;m<20;m++){
		printf("  %d     %d      %d     %d      %d      %d        %d       %d        %d       %d\n", m, processTable[m].occupied, processTable[m].pid, processTable[m].startSeconds, processTable[m].startNano, processTable[m].serviceTimeSec, processTable[m].serviceTimeNano, processTable[m].eventWaitSec, processTable[m].eventWaitNano, processTable[m].blocked);
		fprintf(file, "  %d     %d      %d     %d      %d      %d        %d       %d        %d       %d\n", m, processTable[m].occupied, processTable[m].pid, processTable[m].startSeconds, processTable[m].startNano, processTable[m].serviceTimeSec, processTable[m].serviceTimeNano, processTable[m].eventWaitSec, processTable[m].eventWaitNano, processTable[m].blocked);
	}
}
//help function:
void help(){
	printf("The options for the program are:\n");
	printf("-n <number>   this sets the number of processes to launch\n");
	printf("-s <number>   this sets how many processes can run at once\n");
	printf("-t <number>   this sets the maximum time in seconds a random number generator can chose from for the lifespan of the worker processes\n");
	printf("-f <\"output_file_name.txt\">   this sets the name of the output file\n");
	printf("example:\n./oss -n 3 -s 2 -t 3 -f \"output.txt\"\n");
}
//Message queue constants:
#define PERMS 0777
typedef struct msgbuffer {
	long mtype;
	int intData;
} msgbuffer;

int main(int argc, char** argv){
//initianlze components
	time_t startingTime = time(NULL);
	time_t currentTime;
	srand(time(NULL));
	int option;
	int numWorkers = 0;
	int workerLimit = 0;
	int timePassedIn = 0;
	char *logFile= "log_file.txt";
	//User argument menu:
	while((option = getopt(argc, argv, "hn:s:t:f:")) != -1){
		switch(option){
			case 'h':
				help();
				return EXIT_SUCCESS;
			case 'n':
				numWorkers = atoi(optarg);
				break;
			case 's':
				workerLimit = atoi(optarg);
				break;
			case 't':
				timePassedIn = atoi(optarg);
				break;
			case 'f':
				logFile = optarg;
				break;
			case '?':
				if ((optopt = 'c')){
					printf("Option %c requires an argument\n", optopt);
				} else if (isprint(optopt)){
					printf("Unknown Character '\\x%x'.\n", optopt);
				}
				return 1;
			default:
				help();
				return EXIT_SUCCESS;
		}
	}
	//open output file
	FILE *outputFile = fopen(logFile, "w");
	if (outputFile==NULL){
		printf("Error opening output file!\nTerminating program\n");
		return EXIT_FAILURE;
	}
	//message que initial implementation:
	msgbuffer buf1;
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
	printf("Message  Queue Acitve in Parrent\n");
	fprintf(outputFile,"Message  Queue Acitve in Parrent\n");
	//print user input verification:
	printf("OSS: Number of workers Selected: %d\nNumber of Workers at a time: %d\nNumber of loops for each Worker: %d\nOutput file: %s\n", numWorkers, workerLimit, timePassedIn,logFile);
	fprintf(outputFile, "OSS: Number of workers Selected: %d\nNumber of Workers at a time: %d\nNumber of loops for each Worker: %d\nOutput file: %s\n", numWorkers, workerLimit, timePassedIn,logFile);
	int i=0,j=0,n=0;
	//fork calls:
	pid_t childPid;
	msgbuffer rcvbuf;
	int totalNewWorkers = 0;
	int activeWorkers = 0;
	int termFlag1 = 0;
	int termFlag2 = 0;
	int planToSchedule = 20;
	int totalSecActive;
	int totalNanoActive;
	int printCountNano = 0;
	int printCountSec = 0;
	int tCountNano = 0;
	int tCountSec = 0;
	int tFlag = 0;
	double secRatio;
	double nanoRatio;
	double lowSecRatio = 1;
	double lowNanoRatio = 1;
printf("Entering Parent Loop:\n");
	//while(termination flags are not set) loop:
	while (!(termFlag1==1 && termFlag2==1)){
	//check for active workers in pcb: if none, increment time by -t, else increment by less. set 1 of 2 termination flags
	//also, checked blocked processes to see if unblocked time has passed and act accordingly
	termFlag1 = 0;
		activeWorkers = 0;
		planToSchedule = 20;
		lowSecRatio = 1;
		lowNanoRatio = 1;
		for(i=0;i<20;i++){
			//if occupied and blocked, check time for unblock, if time then unblock
			if(processTable[i].occupied == 1){
				activeWorkers++;

				if(processTable[i].blocked == 1){
					//check to see if time for unblocking has passed:
					if((sysClockSec >= processTable[i].eventWaitSec) && (sysClockNano > processTable[i].eventWaitNano)){
						processTable[i].blocked = 0;
						incrementByX(5000);
 					}
				}

				if(processTable[i].blocked == 0){
					//check time ratio to see if it beats the lowest, if so, it becomes the next scheduled
					totalSecActive = sysClockSec - processTable[i].startSeconds;
					totalNanoActive = sysClockNano - processTable[i].startNano;
					if (totalSecActive == 0){
						secRatio = 0;
					}else{
						secRatio = processTable[i].serviceTimeSec / totalSecActive;
					}
					if (totalNanoActive == 0){
						nanoRatio = 0;
					}else{
						nanoRatio = processTable[i].serviceTimeNano / totalNanoActive;
					}
					if((secRatio < lowSecRatio) && (nanoRatio < lowNanoRatio)){
						planToSchedule = i;
						lowSecRatio = secRatio;
						lowNanoRatio = nanoRatio;
					}
				}
			}
		}

		if ((activeWorkers == 0)){
			//increment by -t time initially and to allow a worker to fork. set flag incase worker max has been hit
			termFlag1 = 1;
			incrementByX(timePassedIn);
		}else{
			incrementByX(50000);
		}

		//message workers by least ammount of runtime
		if ((planToSchedule != 20)) {
			printf("Preparing to send message to %d\n", planToSchedule);
			fprintf(outputFile,"Preparing to send message to %d\n", planToSchedule);
			//send and recieve a message with the selected pcb entry:
			buf1.mtype = processTable[planToSchedule].pid;
			buf1.intData = schTime;
			printf("OSS: Sending message to worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			fprintf(outputFile, "OSS: Sending message to worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			if ((msgsnd(msqid, &buf1, sizeof(msgbuffer)-sizeof(long), 0)) == -1) {
				perror("msgsnd to child failed");
				exit(1);
			}
			printf("OSS: Recieving message from worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			fprintf(outputFile, "OSS: Recieving message from worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			if (msgrcv(msqid, &rcvbuf, sizeof(msgbuffer), getpid(), 0) == -1){
				perror("failed to recieve message in parent\n");
				exit(1);
			}
			//action based on workers response message: including pcb uptates!!
			if (rcvbuf.intData < 0){
				printf("OSS: Worker %d PID %d is planing ot terminate.\n", planToSchedule, processTable[planToSchedule].pid);
				fprintf(outputFile, "OSS: Worker %d PID %d is planing ot terminate.\n", planToSchedule, processTable[planToSchedule].pid);

				incrementByX((rcvbuf.intData * (-1)));
				processTable[planToSchedule].serviceTimeNano += (rcvbuf.intData * -1);//redundant, consider a function instead..
				if ((processTable[planToSchedule].serviceTimeNano >= 1000000000)){
					processTable[planToSchedule].serviceTimeSec++;
					processTable[planToSchedule].serviceTimeNano -= 1000000000;
				}
				activeWorkers--;
				processTable[planToSchedule].occupied = 0;
			}else if(rcvbuf.intData < schTime){

				printf("OSS: Worker %d PID %d is requesting an IO opperation.\n", planToSchedule, processTable[planToSchedule].pid);
				//Must respond with time used
				incrementByX(rcvbuf.intData);
				processTable[planToSchedule].serviceTimeNano += rcvbuf.intData;
				if ((processTable[planToSchedule].serviceTimeNano >= 1000000000)){
					processTable[planToSchedule].serviceTimeSec++;
					processTable[planToSchedule].serviceTimeNano -= 1000000000;
				}
				processTable[planToSchedule].blocked = 1;
				//determine how long the IO opperation will make the program wait
				processTable[planToSchedule].eventWaitSec = (sysClockSec + randSeconds(5));
				processTable[planToSchedule].eventWaitNano = (sysClockNano + randNano());
				//if above 1 sec, add 1 to sec
				if ((processTable[planToSchedule].eventWaitNano > 1000000000)){
					processTable[planToSchedule].eventWaitSec++;
					processTable[planToSchedule].eventWaitNano -= 1000000000;
				}
			}else{
				//full time used
				printf("OSS: Worker %d PID %d finished with it's scheduled time.\n", planToSchedule, processTable[planToSchedule].pid);
				incrementByX(schTime);
				processTable[planToSchedule].serviceTimeNano += schTime;
				if ((processTable[planToSchedule].serviceTimeNano >= 1000000000)){
					processTable[planToSchedule].serviceTimeSec++;
					processTable[planToSchedule].serviceTimeNano -= 1000000000;
				}
			}
		}
		//use logic to see if a new process could/should be forked.if so, set new nano to 1, if total has  launched, set a kill flag.
		//if can create worker, create worker and update PCB:

		// -t time needs to pass, and worker simultaneous and max limits must not be passed
		tFlag = 0;
		if(floor(sysClockNano / timePassedIn) > tCountNano){
			tCountNano++;
			if(tCountNano == floor(timePassedIn/1000000000)){
				tCountNano = 0;
			}
			tFlag = 1;
		}else if(sysClockSec > tCountSec){
			tCountSec++;
			tFlag = 1;
		}

		currentTime = time(NULL);

		if(currentTime - startingTime >= 3){
			termFlag2=1;
		} else if ((totalNewWorkers < numWorkers) && (tFlag == 1)){  //and -t time passed  timePassedIn
			if (activeWorkers < workerLimit){
				childPid = fork();
				if (childPid == -1){
					printf("Fork Process Failed!\n");
					return EXIT_FAILURE;
				}
				//child side of the fork if
				if (childPid == 0) {
					printf("oss fork of procerss %d successful\n", getpid());
					fprintf(outputFile, "oss fork of procerss %d successful\n", getpid());
					int timeSec = randSeconds(timePassedIn);
					int timeNano = randNano();
					char secArg[10];
					char nanoArg[10];
					sprintf(secArg, "%d", timeSec);
					sprintf(nanoArg, "%d", timeNano);
					char * args[] = {"./worker", secArg, nanoArg, NULL};
					execvp("./worker", args);
				}
				//parent side of fork if
				totalNewWorkers++;
				//update pcb entry after a fork:
				processTable[n].occupied = 1;
				processTable[n].pid = childPid;
				processTable[n].startSeconds = sysClockSec;
				processTable[n].startNano = sysClockNano;
				n++;
			}
		}else{
			termFlag2 = 1;
		}

		//Print pcb log every 1/2 second
		if(floor(sysClockNano/500000000)>printCountNano){
			printCountNano++;
			if(printCountNano==2){
				printCountNano = 0;
			}
			printPCB(sysClockSec, sysClockNano, outputFile);
		}else if(sysClockSec>printCountSec){
			printCountSec++;
			printPCB(sysClockSec, sysClockNano, outputFile);
		}
	//increment time
		incrementClock();

	//end loop
	}

printf("OSS Main loop has ended.");

//End with report on average wait time , avg cpu utilization, avg time a process waited in a blocked queue
//also include cpu idle time (no ready processes)
	//End Report:
	//total process time
	double tptS=0;
	double tptN=0;
	//avg wait time
	double awtS=0;
	double awtN=0;
	//avg cpu util
	double acuS=0;
	double acuN=0;
	//cpu idle time
	double citS=0;
	double citN=0;
	for(i=0;i<20;i++){
	//go through the pcb table to determine the average values above
		//add up wait times, divide by total processes launched (total new processes?)
		awtS += processTable[i].eventWaitSec;
		awtN += processTable[i].eventWaitNano;
		if (awtN>999999999){
			awtN -= 1000000000;
			awtS++;
		}

		//add up total process time, remove that from total time for ratio (and avg idle time too)
		tptS += processTable[i].serviceTimeSec;
		tptN += processTable[i].serviceTimeNano;
		if (tptN>999999999){
			tptN -= 1000000000;
			tptS++;
		}
	}

	printf("\n-END REPORT-\n");
	fprintf(outputFile,"\n-END REPORT-\n");

	awtN /= totalNewWorkers;
	awtS /= totalNewWorkers;
	printf("Average Wait Time: %f Seconds %f Nano\n", awtS, awtN);
	fprintf(outputFile,"Average Wait Time: %f Seconds %f Nano\n", awtS, awtN);

	acuN = tptN/sysClockNano;
	if (acuS == 0){
		acuS = 0;
	}else{
		acuS = tptS/sysClockSec;
	}
	citS = sysClockSec - tptS;
	citN = sysClockNano - tptN;
	printf("Average CPU Utilization: %f %% of all Seconds  %f %% of all Nano\n", acuS, acuN);
	fprintf(outputFile,"Average CPU Utilization: %f %% of all Seconds  %f %% of all Nano\n", acuS, acuN);

	printf("Average Time in Blocked Queue is the same as wait time for this program\n");
	fprintf(outputFile,"Average Time in Blocked Queue is the same as wait time for this program\n");

	printf("CPU Idle Time: %f Seconds  %f Nano\n", citS, citN);
	fprintf(outputFile,"CPU Idle Time: %f Seconds  %f Nano\n", citS, citN);


	//close output file:
	fclose(outputFile);
	//clear message ques:
	printf("End of OSS: Message queue is shutting down\n");
	fprintf(outputFile, "End of OSS: Message queue is shutting down\n");
	if (msgctl(msqid, IPC_RMID, NULL) == -1){
		perror("msgctl failed to get rid of que in parent ");
		exit(1);
	}
}
