/* Author: Marley Brandon
 * Date: September 13, 2023*/
#include<unistd.h>
#include<sys/types.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/msg.h>
#include<string.h>
#include<errno.h>

#define SHMKEY 12345
#define BUFF_SZ 2*sizeof(int)
#define PERMS 0644

typedef struct msgbuffer 
{
	long mtype;
	int intData;
	pid_t pid;
	int row;
} msgbuffer;

int main(int argc, char** argv) 
{
	msgbuffer buf;
	buf.mtype = 1;
	int msqid = 0;
	key_t key;

	//get a key for our message queue
	if ((key = ftok("msgq.txt", 1)) == -1)
	{
		perror("ftok");
		exit(1);
	}

	//create our message queue
	if ((msqid = msgget(key, PERMS)) == -1)
	{
		perror("msgget in child");
		exit(1);
	}

	printf("Child %d has access to the queue\n", getpid());



	//grabs shared memory id
	int shmid = shmget ( SHMKEY, BUFF_SZ, 0777);

	//checks to make sure shmid was properly created
        if ( shmid == -1)
        {
                fprintf(stderr, "Child: ... Error in shmget ..\n");
                exit(1);
        }

	//creates pointer that points to shared memory
        int * arr = (int *)(shmat (shmid, 0, 0));
		
	int sec = arr[0];

	 //prints out that worker is just now starting
	 printf("Worker PID: %d PPID: %d SysClockS: %d SysclockNano: %d\n--Just Starting\n", getpid(), getppid(), arr[0], arr[1]);


	 int random, randGen = getpid();
	 srand(randGen);
	int memoryReferences = 0;
	//while loop will hold until termination time has been reached
	while (1)
	{

		//this variable holds the value of the memory address 
		int memoryRequest = rand() % 32768;
		randGen += 1;
//		printf("Memory Request: %d\n", memoryRequest);
		//random value to determine if read or write
		int random = rand() % 100;

		//if 1 read if 0 write
		int row;

		
		if (random >= 30)
		{
			row = 1;
//			printf("read\n");
		}
		else
		{
			row = 0;
//			printf("write\n");
		}

		buf.mtype = getppid();
		buf.pid = getpid();
		buf.intData = memoryRequest;
		buf.row = row;

//		printf("buf row: %d\n", buf.row);

		if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1)
		{
			perror("msgsnd to parent failed");
			exit(1);
		}

		msgbuffer receive;
		
		if (msgrcv(msqid, &receive, sizeof(msgbuffer), getpid(), 0) == -1)
		{
			perror("failed to receive message from parent");
			exit(1);
		}
		
		memoryReferences++;

		//if memory reference > 950 then check to see if terminate
		if (memoryReferences > 950)
		{
//			printf("Random: %d\n", random);
			memoryReferences = 0;
			if (random % 100 == 0)
			{/*
				buf.mtype = getppid();
				buf.pid = getpid();
				buf.intData = -1;
				if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1)
				{
					perror("msgsnd to parent failed");
					exit(1);
				}
			*/
				break;
			}
		}
	}

	//prints an update letting user know worker is terminating because the time was reached
	printf("Worker PID: %d PPID: %d SysClockS: %d SysclockNano: %d\n--Terminating\n", getpid(), getppid(), arr[0], arr[1]);

	//detaching the pointer
        shmdt(arr);
	return EXIT_SUCCESS;
}
