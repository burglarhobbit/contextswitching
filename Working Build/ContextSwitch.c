#include <stdio.h> //for printf
#include <pthread.h> //for threads, mutual exclusion, conditional wait and signalling
#include <time.h> //for accurate clock time
#include <limits.h> //for min-max limits of several data types
#include <inttypes.h> //for using uint64_t data type
#include <stdlib.h> //for malloc
//#include<semaphore.h>
#include  <stdbool.h> //for boolean variables
#include <string.h> // for string operations
#include <signal.h> // for signalling
#include "decode.h"
#define nothread 4 // number of threads running

#define TIMESLICE 50000000 //nanoseconds to define milliseconds
#define BILLION 1000000000L
#define GLOBAL_INIT_RESOURCE_PATH "./initResource.txt"

void *ThreadFunction(void *arg);
void *Dispatcher(void *arg);
void *CopyFile(char *from,char *to);

/* declare catchint, later to be used as the handler */
void catchint(int);

struct Node {
	pthread_t t;
	int tid;
	struct Node *next;
}*startq,*rearq;

struct Node* createNode(int id) {
	struct Node* a = malloc(sizeof(struct Node));
	a->tid=id;
	a->next=NULL;
	return a;
}

struct Node* pop() {
	struct Node* temp=startq;
	if(startq==rearq)
		startq=rearq=NULL;
	else
		startq= startq->next;

	return temp;
}

void push(struct Node* temp) {
	if(startq==NULL)
		startq=rearq=temp;
	else {
		rearq->next=temp;
		rearq=temp;
	}
}

typedef struct ThreadControlBlock {
	bool blockedState,blocked,readyState;
	bool exitStatus;
	char *filePath;
	char *stackFileName;
	int processID;
} TCB;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_t tid[2];
pthread_t disp; //dispatcher
uint64_t diff;
int currentThread;
struct timespec start, end;
int numProcWait = nothread; //No of processes running
TCB process[nothread];

void *ThreadFunction(void *arg) {
	int i,instructions = INT_MAX;
	bool unlockCheck=true; // locked for synchronizing the thread withing itself
	char buff[100];
	uint64_t diff=0;

	TCB *process = (TCB *)arg;
	int fileCount = sizeof(process->filePath)/sizeof(process->filePath);
	FILE *fp;

	char *fileName;

	char *stackFileName = malloc(sizeof(char) * (strlen(process->stackFileName)));
	strcpy(stackFileName,process->stackFileName);

	printf("\nWaiting to start Thread id: %lu\n",pthread_self());

	// Global Init Resource Path
	char *girp = "./initResource.txt";
	CopyFile(girp,stackFileName);

	fileName = malloc(sizeof(char) * strlen(process->filePath));
	strcpy(fileName,process->filePath);
	//printf("FILE NAME: %s\n",fileName);
	fp = fopen(fileName, "r");

	int insSize=0;
	char c = 0,*tmp;

	process->readyState = true;
	while(process->blockedState);
	printf("Received Signal Successful! Thread ID: %lu.\n",pthread_self());

	for (i = INT_MIN; i < instructions; i++) {
		if(process->blockedState) {
			process->blocked = true;
			printf("\nBlocking Thread ID: %lu.\n",pthread_self());
			// Wait until the process is in blocked state
			while(process->blockedState);

			process->blocked = false;
			printf("\nCurrent executing Thread ID: %lu.\n",pthread_self());
		}
		else {
			process->blocked = false;
			if(c != EOF) {
				pthread_t insExe;
				int insStatus;

				while(c!='\n') {
					c = getc(fp);
					insSize++;
				}

				fseek(fp,-(insSize),SEEK_CUR);
				tmp = (char *)malloc(insSize*sizeof(char));
				fread(tmp,insSize,sizeof(char),fp);
				pri((void *)tmp,pthread_self());
				c = getc(fp);
				insSize=1;
			}
			else {
				process->exitStatus = true;
				process->blocked = true;
				printf("Breaking!");
				break;
			}
		}
	}
	numProcWait--;
	free(fileName);
	fclose(fp);

	printf("\nEXITING THREAD: %lu\n",pthread_self());
	//printf("Number of processes waiting: %d\n\n",numProcWait);
	return NULL;
}

void *Dispatcher(void *arg) {
	diff = 0;
	TCB *process = (TCB *) arg;
	char *RESOURCE_PATH = "./resource.txt";

	//printf("STATE1: %s\n",*blockedState?"true":"false");
	//printf("STATE2: %s\n",*(blockedState+1)?"true":"false");
	while(!(process - 1 + startq->tid)->readyState);
	CopyFile((process - 1 + startq->tid)->stackFileName,RESOURCE_PATH);

	(process -1 + startq->tid)->blockedState = false;
	clock_gettime(CLOCK_MONOTONIC, &start);
	while(numProcWait>0) {

		clock_gettime(CLOCK_MONOTONIC, &end);
		diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
		if(diff > TIMESLICE) {
				printf("\nTIMESLICE\n");
				pthread_mutex_lock(&lock);
				//printf("%d\n",startq->tinumProcWait--;d);

				// block the current process
				(process - 1 + startq->tid)->blockedState = !(process -1 + startq->tid)->blockedState;
				// wait for the current process to finish the current executing instruction and get blocked
				while(!(process - 1 + startq->tid)->blocked);

				// Save file from Resource.txt to StackFile[ProcessID].txt for current process
				CopyFile(RESOURCE_PATH,(process - 1 + startq->tid)->stackFileName);

				struct Node *curr = pop();
				if((process - 1 + curr->tid)->exitStatus == false) {
					printf("Pushing %d\n", curr->tid);
					push(curr);
				}
				else {
					printf("\n\nEXITING CURR: %d\n\n", curr->tid);
				}
				printf("Unblocking %d\n", startq->tid);
				// Restore file from StackFile[ProcessID].txt to Resource.txt for next process
				CopyFile((process - 1 + startq->tid)->stackFileName,RESOURCE_PATH);
				(process - 1 + startq->tid)->blockedState = !(process -1 + startq->tid)->blockedState;

				clock_gettime(CLOCK_MONOTONIC, &start);
				pthread_mutex_unlock(&lock);
		}
	}
}

void *CopyFile(char *from, char *to) {
	char *fromFile = malloc(sizeof(char)*strlen(from));
	char *toFile = malloc(sizeof(char)*strlen(to));

	strcpy(fromFile,from);
	strcpy(toFile,to);

	FILE *read = fopen(fromFile,"r");
	char c = getc(read);
	int count=1;
	while(c!=EOF) {
		c = getc(read);
		count++;
	}
	rewind(read);
	char *buff = (char *)malloc(count*sizeof(char));
	int i=1;
	for(i = 0;i<count;i++)
		buff[i] = getc(read);
	fclose(read);

	FILE *write = fopen(toFile,"w");
	i = 0;
	c = buff[i];
	while(c!=EOF) {
		fputc(c,write);
		i++;
		c = buff[i];
	}
	fclose(write);

	free(fromFile);
	free(toFile);
}

void catchint(int signo) {
	pthread_mutex_lock(&lock);
	//printf("%d\n",startq->tid);
	char *RESOURCE_PATH = "./resource.txt";
	printf("\nInterrupt Subrouting: CHANGING THREAD!\n");
	// block the current process
	(process - 1 + startq->tid)->blockedState = !(process -1 + startq->tid)->blockedState;
	// wait for the current process to finish the current executing instruction and get blocked
	while(!(process - 1 + startq->tid)->blocked);

	// Save file from Resource.txt to StackFile[ProcessID].txt for current process
	CopyFile(RESOURCE_PATH,(process - 1 + startq->tid)->stackFileName);

	struct Node *curr = pop();
	while((process - 1 + startq->tid)->exitStatus == true)
		pop();

	// Restore file from StackFile[ProcessID].txt to Resource.txt for next process
	CopyFile((process - 1 + startq->tid)->stackFileName,RESOURCE_PATH);

	(process - 1 + startq->tid)->blockedState = !(process -1 + startq->tid)->blockedState;
	push(curr);

	clock_gettime(CLOCK_MONOTONIC, &start);
	pthread_mutex_unlock(&lock);
}


int main() {

	startq=rearq=NULL;

	for(int i=0;i<nothread;++i) {
		struct Node* temp = createNode(i+1);
		push(temp);
	}
	struct timespec startfinal, endfinal;

	for(int i=0;i<nothread;++i) {
		process[i].blockedState = true;
		process[i].readyState = false;
		process[i].blocked = true;
		process[i].exitStatus = false;
		process[i].exitStatus = false;
		process[i].processID = i;
	}

	process[0].filePath = "./filein_1.txt";
	process[0].stackFileName = "./stack_filein_1.txt";

	process[1].filePath = "./filein_2.txt";
	process[1].stackFileName = "./stack_filein_2.txt";

	process[2].filePath = "./filein_3.txt";
	process[2].stackFileName = "./stack_filein_3.txt";

	process[3].filePath = "./filein_4.txt";
	process[3].stackFileName = "./stack_filein_4.txt";
	clock_gettime(CLOCK_MONOTONIC, &startfinal);

	static struct sigaction act;
	struct Node *iter = startq;
    /* set up the action to be taken on receipt of SIGNINT */
    act.sa_handler = catchint;

    /*create a full mask */
    sigfillset(&(act.sa_mask));

    /*before sigaction call, SIGINT will terminate process (defualt action) */
    sigaction(SIGINT, &act, NULL);
    /* on receipt of SIGINT control will be passed to catchint */

	pthread_create(&disp, NULL, Dispatcher, &process);
	for(int i=0;i<nothread;++i,iter=iter->next) {
		pthread_create(&(iter->t), NULL, ThreadFunction, (void *)&process[iter->tid-1]);
	}

	struct Node *iter2=startq;
	for(int i=0;i<nothread;++i,iter2=iter2->next) {
		pthread_join(iter2->t, NULL);
		//printf("Thread %d EXITED. No. of Waiting Processes: %d\n",i+1,numProcWait);
	}

	clock_gettime(CLOCK_MONOTONIC, &endfinal);
	diff = BILLION * (endfinal.tv_sec - startfinal.tv_sec) + endfinal.tv_nsec - startfinal.tv_nsec;
	printf("Total Run Time: %" PRIu64 " ms\n",diff/1000000);

	pthread_mutex_destroy(&lock);
}