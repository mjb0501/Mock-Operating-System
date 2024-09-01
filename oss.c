/*Author: Marley Brandon
 * Date: September 13, 2023*/
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
#include "oss.h"

#define SHMKEY 12345
#define BUFF_SZ 2*sizeof(int)
#define PERMS 0644


int main(int argc, char** argv) 
{
	signal(SIGALRM, alarmSignalHandler);
	alarm(5);
	//this signal will call the signal handler if ctr + c is entered
	signal(SIGINT, myhandler);
	
	//if statements ensure that the 60 second interupt is setup properly
	if (setupinterrupt() == -1)
	{
		perror("Failed to set up handler for SIGPROF");
		return -1;
	}

	if (setupitimer() == -1)
	{
		perror("Failed to set up the ITIMER_PROF interval timer");
		return -1;
	}

//	int msqid;
	key_t key;
	system("touch msgq.txt");

	//get a key for our message queue
	if ((key = ftok("msgq.txt", 1)) == -1)
	{
		perror("ftok");
		exit(1);
	}

	//create our message queue
	if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1)
	{
		perror("msgget in parent");
		exit(1);
	}

	//seed is created to ensure that all times randomly generated are truly random
	srand(time(0));
	int seed = rand();


/*proc holds the number of processes to be run, simul holds the number of simultaneous processes, and timer holds the max time each child is to run*/
	int proc = 0, simul = 0, timer = 0, option;
	char file[100];
	bool verbose = false;
	
	//creating shared memory
	sharedMemory.shmid = shmget(SHMKEY, BUFF_SZ, 0777 | IPC_CREAT);
	//checks to ensure that shared memory was properly created
	if ( sharedMemory.shmid == -1)
	{
		fprintf(stderr,"Parent: ... Error in shmget ..\n");
		exit(1);
	}

	//creates pointer attached to shared memory
	sharedMemory.arr = (int * )(shmat(sharedMemory.shmid, 0, 0));


	//Get opt statement that parses input and gives assigns variables the values provided in inupt
	while ( (option = getopt(argc, argv, "hvn:s:t:f:")) != -1)
	{
		switch(option)
		{
			case 'h':
				help();
				return EXIT_SUCCESS;
			case 'v':
				verbose = true;
				break;
			case 'n':
				proc = atoi(optarg);
				break;
			case 's':
				simul = atoi(optarg);
				break;
			case 't':
				timer = atoi(optarg);
				break;
			case 'f':
				strcpy(file, optarg);
				break;
			case '?':
				return EXIT_FAILURE;
			default:
				help();
				return EXIT_FAILURE;
		}
	}
	
	/*Checks to make sure that a value was provided for proc, simul, timer, and file.
	 * If Values are not provided then it will return a message alerting them, and letting them know how to run the program, and then will stop running the program.*/
	if (proc == 0 || simul == 0 || timer == 0 || strlen(file) == 0)
	{
		printf("Error either the number of processes, simultaneous processes, timelimit, or file was not provided\n");
		help();
		return EXIT_SUCCESS;
	}

	//opening file
//	FILE *logFile;
	logFile = fopen(file, "w");

	//numchild holds the current number of child processes running
	int numchild = 0;
	//childCount holds the number of children ran in total
	int childCount = 0;
	
	//flag will be 1 if child terminated
	//flag will be 0 if child did not terminate
	int flag = 1;

	int timerSecs, timerNano = 0;

	int halfSec = 0, halfNano = 500000000;

	int second = 1;
	//used for outputing resource table every 
	int x = 0;

	//statistics used for statistic report
	int memoryAccess = 0, pageFaults = 0;
	long long emat[proc];
	int accessTimes[proc];

	for (int i = 0; i < proc; i++)
	{
		emat[i] = 0;
		accessTimes[i] = 0;
	}

	struct pageTable pageTable[proc];

	//initializes page table
	for (int i = 0; i < proc; i++)
	{
		for (int j = 0; j < 32; j++)
		{
			pageTable[i].pages[j] = -1;
		}
	}

	struct frame frameTable[256];

	for (int i = 0; i < 256; i++)
	{
		frameTable[i].occupied = false;
		frameTable[i].dirtyBit = 0;
		frameTable[i].head = false;
		frameTable[i].process = -1;
		frameTable[i].page = -1;
	}
	
	//keeps the count of the lines printed to logfile
	int lineCount = 0;

	//this value correlates to the head of the fifo
	int head = 0;

	//while there are no longer children to be forked or there are no children currently running
	while ((childCount < proc && !fiveSec) || (numchild != 0))
	{
//		printf("%d", numchild);
		msgbuffer buf;
		buf.mtype = 1;
//		printf("Parent is looping\n");
		//CREATES A PASSIVE WAIT CALL
		int status;
                int pid = waitpid(-1, &status, WNOHANG);
		//releases resources held by terminated process
		if (pid > 0)
		{
			fprintf(logFile, "Child terminated");
			printf("child termination detected\n");
			int process;
			for (int i = 0; i < 30; i++)
			{
				if (processTable[i].pid == pid)
				{
					fprintf(logFile, "Effective memory access time: %lld nanoseconds\n", emat[i] / (long long)accessTimes[i]);
//					fprintf(logFile, "emat: %lld\naccessTimes: %d\n", emat[i], accessTimes[i]);
					//clears out pageTable entry for process
					for (int j = 0; j < 32; j++)
					{
						pageTable[i].pages[j] = -1;
					}
					processTable[i].occupied = 0;
					//clears out frames holding process
					for (int j = 0; j < 256; j++)
					{
						if (frameTable[j].process == pid)
						{
//							printf("Removing process %d page %d from queue", i, j);
							frameTable[j].occupied = false;
							frameTable[j].dirtyBit = 0;
							frameTable[j].process = -1;
							frameTable[j].page = -1;
							rmvQueue(j);
							
							/*
							for (int e = 0; e < 256; e++)
							{
								printf("%d ", waitQueue[e]);
							}
							printf("\n");
							*/
						}
					}
					numchild--;
				}
			}
		}

		//launches a new process as necessary
		while (childCount < proc && !fiveSec && numchild < simul && ((timerSecs <= sharedMemory.arr[0] && timerNano <= sharedMemory.arr[1]) || (timerSecs < sharedMemory.arr[0]) || numchild == 0) && childCount < 100 && numchild < 19)
		{
//			printf("\n\nChild loop entered at %d:%d\n\n", sharedMemory.arr[0], sharedMemory.arr[1]);
			timerNano += timer;

			if (timerNano >= billion)
			{
				timerSecs += 1;
				timerNano -= billion;
			}

			numchild += 1;

			processTable[childCount].occupied = 1;
			processTable[childCount].startSeconds = sharedMemory.arr[0];
			processTable[childCount].startNano = sharedMemory.arr[1];

			childCount++;

			pid_t childpid = fork();

			processTable[childCount - 1].pid = childpid;

			if (childpid == 0)
			{
//				printf("---------------------------\nI am a child %d\n-----------------------------\n", childCount);
				char* args[] = {"./worker", NULL};
				execvp("./worker", args);
				fprintf(stderr, "Exec failed, terminating\n");
				exit(1);
			}
		}
		
		//this variable will count amount of processes waiting for memory
		int waitCount = 0;
		
		//loop will iterate through each blocked process and determine if it is time for it to be unblocked
		for (int i = 0; i < childCount; i++)
		{
			if (processTable[i].blocked)
			{
				if (processTable[i].request == -1)
				{
					fprintf(logFile, "Request is negative 1\n");
				}
//				printf("blocked loop");
//				fprintf(logFile, "Detected process was blocked\n");
				if (processTable[i].waitSec < sharedMemory.arr[0] || (processTable[i].waitSec <= sharedMemory.arr[0] && processTable[i].waitNano < sharedMemory.arr[1]))
				{
					int empty = 0;
					int emptyFrame = -1;
					for (int i = 0; i < 256; i++)
					{
						if (!frameTable[i].occupied)
						{
							empty = 1;
							emptyFrame = i;
							break;
						}
					}
					
					if (empty)
					{

						fprintf(logFile, "OSS: After having to wait due to a page fault, P%d's request %d was placed in empty frame %d at time %d:%d\n", i, processTable[i].request, emptyFrame, sharedMemory.arr[0], sharedMemory.arr[1]);
						frameTable[emptyFrame].process = processTable[i].pid;
						frameTable[emptyFrame].page = processTable[i].request / 1024;
						frameTable[emptyFrame].occupied = true;

						if (processTable[i].requestRow == 0)
						{
							frameTable[emptyFrame].dirtyBit = 1;
						}

						
						pageTable[i].pages[processTable[i].request / 1024] = emptyFrame;
						buf.mtype = processTable[i].pid;
						if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1)
						{
							perror("msgsnd to child failed\n");
							exit(1);
						}

						enqueue(emptyFrame);
						empty = 0;
						emptyFrame = -1;

						processTable[i].blocked = 0;
						processTable[i].waitSec = 0;
						processTable[i].waitNano = 0;
						processTable[i].request = -1;
						processTable[i].requestRow = -1;
					}
					else
					{
						int blocked;
	
						fprintf(logFile, "OSS: After having to wait due to a page fault, P%d's  request %d replaced memory in frame %d at time %d:%d\n", i, processTable[i].request, peek(), sharedMemory.arr[0], sharedMemory.arr[1]);
						//clearing the page table of the process being removed from frame
						for (int j = 0; j < childCount; j++)
						{
							if (processTable[j].pid == frameTable[peek()].process)
							{
								pageTable[j].pages[frameTable[peek()].page] = -1;
								break;
							}
						}
	
						processTable[i].blocked = 0;
						processTable[i].waitSec = 0;
						processTable[i].waitNano = 0;
						pageTable[i].pages[processTable[i].request/1024] = peek();
	//					printf("Page %d is set to %d\n", i, peek());
						frameTable[peek()].occupied = true;
						if (processTable[i].requestRow == 1)
						{
							frameTable[peek()].dirtyBit = 0;
						}
						else
						{
	//						printf("in dirty bit chooser\n");
							frameTable[peek()].dirtyBit = 1;
						}
						frameTable[peek()].head = false;
						frameTable[peek()].process = processTable[i].pid;
						frameTable[peek()].page = processTable[i].request / 1024;
						processTable[i].request = -1;
						processTable[i].requestRow = -1;
						
	//					printf("This is peek: %d\n", peek());	
						//create this to requeue after removing as front
						int temp = peek();
						dequeue();
						enqueue(temp);
	
						frameTable[peek()].head = true;
	
						for (int i = 0; i < 256; i++)
						{
							if (frameTable[i].head && i != peek())
							{
								frameTable[i].head = false;
							}
						}
	
						buf.mtype = processTable[i].pid;
						if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1)
						{
							perror("msgsnd to child failed\n");
							exit(1);
						}
					}
					break;
				}
				//if the process is still blocked then the wait count is incremented to express that another process is blocked still
				else
				{
					waitCount++;	
				}
			}

			//if all the processes are blocked then increment by a milisecond and reassess all blocked processes 
			if (waitCount >= numchild && numchild > 0)
			{
				waitCount = 0;
				i = 0;
				sharedMemory.arr[1] += 1000000;
				
				if (sharedMemory.arr[1] >= billion)
				{
					sharedMemory.arr[0] += 1;
					sharedMemory.arr[1] -= billion;
				}
			}
		}

					

		//check if we have a message from a child and deal with message accordingly
		msgbuffer message;
		if(msgrcv(msqid, &message, sizeof(msgbuffer), getpid(),IPC_NOWAIT)==-1)
		{
			if(errno != ENOMSG)
			{
				printf("Got an error from msgrcv\n");
				perror("msgrcv");
				exit(1);
			}
		}
		else
		{

			//this is all new stuff that is not finished yet
			if (message.row == 1)
			{
				fprintf(logFile, "OSS: Process %d requesting read of address %d at time %d:%d\n", message.pid, message.intData, sharedMemory.arr[0], sharedMemory.arr[1]);
			}
			else
			{
				 fprintf(logFile, "OSS: Process %d requesting write of address %d at time %d:%d\n", message.pid, message.intData, sharedMemory.arr[0], sharedMemory.arr[1]);
			}
			memoryAccess += 1;
			int alreadyIn = 0;
			for (int i = 0; i < 256; i++)
			{
				if (frameTable[i].process == message.pid && frameTable[i].page == message.intData / 1024)
				{
					alreadyIn = 1;
					if (frameTable[i].dirtyBit != 1);
					{
						fprintf(logFile, "OSS: Dirty bit of frame %d set, adding additional time to the clock\n", i);
						sharedMemory.arr[1] += 300;
					}
				}
			}

			sharedMemory.arr[1] += 100;

//			if (message.intData == -1)
//			{
					

			if (alreadyIn)
			{
				fprintf(logFile, "OSS: Address %d already in the frame table at time %d:%d\n", message.intData,  sharedMemory.arr[0], sharedMemory.arr[1]);
				int index;
				//printf("Already in triggered\n");
				
				if (sharedMemory.arr[1] > billion)
				{
					sharedMemory.arr[0] += 1;
					sharedMemory.arr[1] -= billion;
				}

				for (int i = 0; i < childCount; i++)
                                {
                                        if (processTable[i].pid == message.pid)
                                        {
						emat[i] += 100;
						accessTimes[i]++;
                                                buf.mtype = processTable[i].pid;
                                                if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1)
                                                {
                                                        perror("msgsnd to child failed\n");
                                                        exit(1);
                                                }
                                        }
                                }
			}
			/*
			else if (empty)
			{
				frameTable[emptyFrame].process = message.pid;
				frameTable[emptyFrame].page = message.intData / 1024;
				frameTable[emptyFrame].occupied = true;

//				 printf("This is empty frame: %d\n", emptyFrame);
				if (message.row == 0)
				{
					frameTable[emptyFrame].dirtyBit = 1;
				}

				for (int i = 0; i < childCount; i++)
                                {
                                        if (processTable[i].pid == message.pid)
                                        {
						emat[i] += 100;
						accessTimes[i]++;
                                                pageTable[i].pages[message.intData / 1024] = emptyFrame;
                                                buf.mtype = processTable[i].pid;
                                                if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1)
                                                {
                                                        perror("msgsnd to child failed\n");
                                                        exit(1);
                                                }
                                        }
                                }

				sharedMemory.arr[1] += 100;

                                if (sharedMemory.arr[1] > billion)
                                {
                                        sharedMemory.arr[0] += 1;
                                        sharedMemory.arr[1] -= billion;
                                }

				fprintf(logFile, "OSS: Process %d being moved into empty frame %d at time %d:%d\n", message.pid, emptyFrame, sharedMemory.arr[0], sharedMemory.arr[1]);
				
//				printf("Process found empty frame enqueueing\n");
				enqueue(emptyFrame);
				empty = 0;
				emptyFrame = -1;
			}
			*/
			else
			{
				pageFaults += 1;
                                for (int i = 0; i < childCount; i++)
                                {
                                        if (processTable[i].pid == message.pid)
                                        {
						emat[i] += 14000000;
						accessTimes[i]++;
                                                processTable[i].blocked = 1;
                                                processTable[i].waitNano = sharedMemory.arr[1] + 14000000;

                                                if (processTable[i].waitNano >= billion)
                                                {
                                                        processTable[i].waitSec = sharedMemory.arr[0] + 1;
                                                        processTable[i].waitNano -= billion;
                                                }
                                                else
                                                {
                                                        processTable[i].waitSec = sharedMemory.arr[0];
                                                }

                                                processTable[i].request = message.intData;
                                                processTable[i].requestRow = message.row;
						fprintf(logFile, "OSS: Address %d is not in a frame, pagefault\n", message.pid);
						break;
//                                                printf("message row: %d\n", message.row);
                                        }
                                }
			}
		}


/*
			//if there is not a page fault
			if (!frameTable[head].occupied)
			{
				fprintf(logFile, "Entered no page fault statement\n");
				frameTable[head].occupied = 1;
				if (message.row == 0)
				{
					printf("in dirty bit chooser\n");
					frameTable[head].dirtyBit = 1;
					//increment by a bit more time if write
					sharedMemory.arr[1] += 100;
				}
				frameTable[head].head = false;
				frameTable[head + 1].head = true;
				frameTable[head].process = message.pid;
				frameTable[head].page = message.intData / 1024;

				//have to find the process that is making this request
				for (int i = 0; i < childCount; i++)
				{
					if (processTable[i].pid == message.pid)
					{
						pageTable[i].pages[message.intData / 1024] = head;
						buf.mtype = processTable[i].pid;
						if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1)
						{
							perror("msgsnd to child failed\n");
							exit(1);
						}
					}
				}
				
				head++;

				if (head > 256)
				{
					head = 0;
				}

				sharedMemory.arr[1] += 100;

				if (sharedMemory.arr[1] >= billion)
				{
					sharedMemory.arr[0] += 1;
					sharedMemory.arr[1] -= billion;
				}
			}
			// if there is a page fault
			else
			{
				fprintf(logFile, "Page Fault Detected\n");
				for (int i = 0; i < childCount; i++)
				{
					if (processTable[i].pid == message.pid)
					{
						processTable[i].blocked = 1;
						processTable[i].waitNano = sharedMemory.arr[1] + 14000000;

						if (processTable[i].waitNano >= billion)
						{
							processTable[i].waitSec = sharedMemory.arr[0] + 1;
							processTable[i].waitNano -= billion;
						}
						else
						{
							processTable[i].waitSec = sharedMemory.arr[0];
						}
						
						processTable[i].request = message.intData;
						processTable[i].requestRow = message.row;
						printf("message row: %d\n", message.row);
					}
				}
			}
		}
*/
       
		
//		printf("Numchild: %d\n", numchild);
//		printf("Made it past msgrcv\n");

		//every half second output pcb
		if ((sharedMemory.arr[1] >= halfNano && sharedMemory.arr[0] >= halfSec) || (sharedMemory.arr[0] > halfSec))
		{
			printf("Half Second");
			halfNano += 500000000;
			if (halfNano >= billion)
			{
				halfSec += 1;
				halfNano -= billion;
			}

			fprintf(logFile, "OSS PID:%d SysClockS: %d SysclockNano: %d\nProcess Table:\nEntry      Occupied          PID        StartS           StartN        Blocked        WaitSec        WaitNano\n", getpid(), sharedMemory.arr[0], sharedMemory.arr[1]);
			printf("OSS PID:%d SysClockS: %d SysclockNano: %d\nProcess Table:\nEntry      Occupied          PID        StartS           StartN        Blocked        WaitSec        WaitNano\n", getpid(), sharedMemory.arr[0], sharedMemory.arr[1]);

        		for (int i = 0; i < proc; i++)
        		{
				printf("%-6d     %-11d       %-7d    %-11d      %-6d       %-8d       %-8d        %-8d\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano, processTable[i].blocked, processTable[i].waitSec, processTable[i].waitNano);
                		if (lineCount < lineCap)
				{
					lineCount++;
					fprintf(logFile, "%-6d     %-11d       %-7d    %-11d      %-6d       %-8d       %-8d        %-8d\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano, processTable[i].blocked, processTable[i].waitSec, processTable[i].waitNano);
				}
			}
			
			//prints out the page tables
			for (int i = 0; i < childCount; i++)
			{
				fprintf(logFile, "Process %d Page Table\n-----------------------------------\n", i);
				printf("Process %d Page Table\n-----------------------------------\n", i);
				for (int j = 0; j < 32; j++)
				{
					fprintf(logFile, "Page %d: %d\n", j, pageTable[i].pages[j]);
					printf("Page %d: %d\n", j, pageTable[i].pages[j]);
				}
			}
			
			//prints out memory layout
			fprintf(logFile, "Current memory layout at time %d:%d is:\n         Occupied   DirtyBit  HeadOfFIFO\n", sharedMemory.arr[0], sharedMemory.arr[1]);
			for (int i = 0; i < 256; i++)
			{	
				fprintf(logFile, "Frame %d: %-8d   %-8d  %-10d\n", i, frameTable[i].occupied, frameTable[i].dirtyBit, frameTable[i].head);
			}

			//prints out the frame table
			fprintf(logFile, "Frame Table\n--------------------------------------\n         Process  Page\n");
			printf("Frame Table\n--------------------------------------\n         Process  Page\n");

			for (int i = 0; i < 256; i++)
			{
				int temp = -1;
				for (int j = 0; j < 30; j++)
				{
					if (processTable[j].pid == frameTable[i].process && frameTable[i].occupied)
					{
						temp = j;
						break;
					}
				}
				fprintf(logFile, "Frame %d: %-9d  %-6d\n", i, temp, frameTable[i].page);
				printf("Frame %d: %-9d  %-6d\n", i, temp, frameTable[i].page);
			}
		}
	}
//		printf("Made it past clock incrementer should loop now\n");


	//update process table by making last process occupied = 0
	processTable[proc - 1].occupied = 0;
	//print out process table one last time
/*
	fprintf(logFile, "OSS PID:%d SysClockS: %d SysclockNano: %d\nProcess Table:\nEntry      Occupied          PID        StartS           StartN\n", getpid(), sharedMemory.arr[0], sharedMemory.arr[1]);
	for (int i = 0; i < proc; i++)
        {
		fprintf(logFile, "%-6d     %-11d       %-7d    %-11d      %-6d\n", i, processTable[i].occupied, processTable[i].pid, processTable[i].startSeconds, processTable[i].startNano);
        }

	for (int i = 0; i < childCount; i++)
	{
                for (int j = 0; j < 10; j++)
                {
                        printf("%d ", allocationMatrix[i][j]);
                }
                printf("\n");
        }

        printf("---------------------\n");
        for (int i = 0; i < 10; i++)
        {
                printf("%d ", allocationArray[i]);
        }
        printf("\n-------------------\n");
*/
	
	printf("Statistics:\n---------------------------------\n");
	printf("Memory Accesses Per Second: %f\n", (float)memoryAccess / sharedMemory.arr[0]);
	printf("Page Faults Per Memory Access: %f\n", (float)pageFaults / memoryAccess);


	/* Testing The Enqueue and Dequeue Functions
	enqueue(4);
	enqueue(1);
	enqueue(2);
	enqueue(3);
	dequeue();
	rmvQueue(2);

	for (int i = 0; i < 256; i++)
	{
		printf("%d ", waitQueue[i]);
	}

	printf("\nRear: %d\nFront: %d\n", waitQueue[rear], waitQueue[front]);
	*/


	printf("Ending Program\n");

	//close file
	fclose(logFile);

	//get rid of message queue may also throw error
	if (msgctl(msqid, IPC_RMID, NULL) == -1)
	{
		perror("msgctl to get rid of queue in parent failed");
		exit(1);
	}

	//free up shared memory
	shmdt(sharedMemory.arr);
        shmctl(sharedMemory.shmid,IPC_RMID, NULL);

	return EXIT_SUCCESS;
}
