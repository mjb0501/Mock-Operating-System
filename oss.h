#include<unistd.h>
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<time.h>
#include<errno.h>
#include<sys/time.h>
#include<signal.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<sys/wait.h>
#include<string.h>
#include<stdbool.h>

#define SHMKEY 12345
#define BUFF_SZ 2*sizeof(int)
#define PERMS 0644
#define resources 10
#define instances 20
#define billion 1000000000
#define lineCap 100000

//this struct represents one process control block item
struct PCB
{
        int occupied;
        pid_t pid;
        int startSeconds;
        int startNano;
	int blocked;
	int waitSec;
	int waitNano;
	int request;
	int requestRow;
};

//this struct holds the shmid and pointer to the shared memory
struct shmem
{
        int shmid;
        int * arr;
};

typedef struct msgbuffer
{
        long mtype;
        int intData;
	pid_t pid;
	int row;
} msgbuffer;

struct pageTable
{
	int pages[32];
};

struct frame
{

	bool occupied;
	int dirtyBit;
	bool head;
	pid_t process;
	int page;
};

//process table declaration
struct PCB processTable[30];

//wait queue
int waitQueue[256];

//rear and front of waitQueue
int rear = -1;
int front = -1;

//struct holding shmid and pointer declaration
struct shmem sharedMemory;
//mesage queue
int msqid;
//File used to write to file
FILE *logFile;

//help function that will print out help statement when called
void help()
{
        printf("To run the program give a command like this: ./oss -v -n 3 -s 1 -t 3 -f log.txt\nAs for the values provided:\nn is number of child processes to be forked\ns is the number of child processes to run simultaneously\nt is time to run in between forking off the next process\nf is the log file that will be written into\nv specifies whether verbose is on or off if -v is entered it is on if not it is off");
}

static void enqueue(int x)
{
	if (front == -1 && rear == -1)
	{
		front = 0;
		rear = 0;
		waitQueue[rear] = x;

	}
	else if ((rear + 1) % 256 == front)
	{
		return;
	}
	else
	{
		rear = (rear + 1) % 256;
		waitQueue[rear] = x;
	}
}

static void dequeue()
{
	if (front == -1 && rear == -1)
	{
		return;
	}
	else if (front==rear)
	{
		front = -1;
		rear = -1;
	}
	else
	{
		front = (front + 1) % 256;
	}
}

static int peek()
{
	if (waitQueue[front] == -1)
	{
		return -1;
	}
	return waitQueue[front];
}

static void rmvQueue(int x)
{
	int index = -1;

	for (int i = 0; i < 256; i++)
	{
		if (waitQueue[i] == x)
		{
			index = i;
			break;
		}
	}
	
	//this may not work since this is a circular array
	if (index != -1)
	{
		if (index != rear)
		{
			for (int i = index; i < 256 - 1; i++)
                        {
                                waitQueue[i] = waitQueue[i+1];
                        }
			rear = (rear - 1) % 256;
		}
		else if (index == rear)
		{
			rear = (rear - 1) % 256;
		}
		else if (index == front)
		{
			front = (front + 1) % 256;
		}
	}
	/*
	for (int i = 0; i < 256; i++)
	{
		if (waitQueue[i] == x)
		{
			waitQueue.splice(i, 1);
		}
	}
	*/
}	

//function handles two types of interupts, if time limit is reached (60 seconds) or if ctrl + c is entered
static void myhandler (int s)
{
        int errsave;
        errsave = errno;

        //loop loops through all child processes and ensures they are terminated
        for (int i = 0; i < (sizeof(processTable) / sizeof(processTable[0])); i++)
        {
                if (processTable[i].occupied = 1 && processTable[i].pid != 0)
                {
                        kill(processTable[i].pid, SIGTERM);
                }
        }

        //close open file
        fclose(logFile);

        //close message queue
        msgctl(msqid, IPC_RMID, NULL);

        //freeing up shared memory
        shmdt(sharedMemory.arr);
        shmctl(sharedMemory.shmid,IPC_RMID,NULL);

        //terminating parent process ending program
        kill(0, SIGTERM);
        errno = errsave;
}

//alarm for not forking new processes
bool fiveSec = false;
void alarmSignalHandler(int signum)
{
	printf("OSS: 5 seconds have passed no new processes will be generated");
	fiveSec = true;
}

//following two functions set up timer that sends interupt if 60 seconds are passed
static int setupinterrupt(void)
{
        struct sigaction act;
        act.sa_handler = myhandler;
        act.sa_flags = 0;
        return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

static int setupitimer(void)
{
        struct itimerval value;
        value.it_interval.tv_sec = 30;
        value.it_interval.tv_usec = 0;
        value.it_value = value.it_interval;
        return (setitimer(ITIMER_PROF, &value, NULL));
}



