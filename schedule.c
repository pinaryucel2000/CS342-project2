#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <fcntl.h> 

struct burst
{
	int threadIndex;
	int burstIndex;
	double length;
	unsigned long generationTime;
	struct burst* next;
};

// Variables required 
int N;
int Bcount;
int minB = 100; 
int avgB;
int minA = 100;
int avgA;
char ALG[10];
char fileName[256];

// Other required variables
struct burst* runQueue;
int burstCount = 0;
int burstCountSoFar = 0;
double* virtualRuntimes;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
int** burstInfo;
int* burstInfoSize;
unsigned long** waitingTimes;

// Function declerations
void randomMode();
void fileMode();

void FCFS();
void SJF();
void PRIO();
void VRUNTIME();

int getBurstCount();
char* itoa(int value, char* buffer);
char* reverse(char *buffer, int i, int j);
void swap(char *x, char *y);
int isDigit(char c);
unsigned long getCurrentTime(); // Retuns the current time in milliseconds
double getRandomA(double avg);
double getRandomB(double avg);
void* generateBurstRandomMode(void* threadIndexPtr);
void* generateBurstFileMode(void* threadIndexPtr);
void info(struct burst* burst);

int  main(int argc, char *argv[])
{
	if(argc != 8 && argc != 5)
	{
		fprintf(stderr, "Invalid number of arguments.\n");
		exit(1);	
	}
	else if(argc == 8)
	{
		if(atoi(argv[1]) < 1 || atoi(argv[1]) > 10 || atoi(argv[2]) < 0 || atoi(argv[3]) < 0 || atoi(argv[4]) < 0 || atoi(argv[5]) < 0 || atoi(argv[6]) < 0)
		{
			fprintf(stderr, "Invalid arguments.\n");
			exit(1);
		}
		
		N = atoi(argv[1]);
		Bcount = atoi(argv[2]);
		
		if(atoi(argv[3]) >= 100)
		{
			minB = atoi(argv[3]);
		}
		else
		{
			printf("Invalid minB value. Default value 100 will be used.\n");	
		}
		
		avgB = atoi(argv[4]);
		
		if(atoi(argv[5]) >= 100)
		{
			minA = atoi(argv[5]);
		}
		else
		{
			printf("Invalid minA value. Default value 100 will be used.\n");	
		}
		
		avgA = atoi(argv[6]);
		
		if(strcmp(argv[7], "FCFS") == 0 || strcmp(argv[7], "SJF") == 0 || strcmp(argv[7], "PRIO") == 0 || strcmp(argv[7], "VRUNTIME") == 0)
		{
			strcpy(ALG, argv[7]);
		}
		else
		{
			fprintf(stderr, "Invalid arguments.\n");
			exit(1);			
		}
		
		randomMode();
	}
	else if(argc == 5)
	{
		if(strcmp(argv[3], "-f") != 0 || atoi(argv[1]) < 1 || atoi(argv[1]) > 10)
		{
			fprintf(stderr, "Invalid arguments.\n");
			exit(1);		
		}
		
		N = atoi(argv[1]);
		
		if(strcmp(argv[2], "FCFS") == 0 || strcmp(argv[2], "SJF") == 0 || strcmp(argv[2], "PRIO") == 0 || strcmp(argv[2], "VRUNTIME") == 0)
		{
			strcpy(ALG, argv[2]);
		}
		else
		{
			fprintf(stderr, "Invalid arguments.\n");
			exit(1);			
		}
		
		strcpy(fileName, argv[4]);
		
		fileMode();
	}
}

void* generateBurstRandomMode(void* threadIndexPtr)
{
	int threadIndex = * (int *)threadIndexPtr;
	free(threadIndexPtr);
	srand(time(NULL) * threadIndex);
	unsigned long generationTime;
	
	int i;
	for(i = 1; i <= Bcount; i++)
	{
		usleep(getRandomA(avgA)*1000);
		generationTime = getCurrentTime();
		struct burst* newBurst = malloc(sizeof(struct burst));
		newBurst->threadIndex = threadIndex;
		newBurst->burstIndex = i;
		newBurst->length = getRandomB(avgB);
		newBurst->generationTime = generationTime;
		
		pthread_mutex_lock(&mutex);
		//printf("Burst created.");
		//info(newBurst);
		newBurst->next = runQueue;
		runQueue = newBurst;	
		pthread_cond_signal(&condition);
		pthread_mutex_unlock(&mutex);
	}

	pthread_exit(0); 
}

void* generateBurstFileMode(void* threadIndexPtr)
{
	int threadIndex = * (int *)threadIndexPtr;
	free(threadIndexPtr);
	unsigned long generationTime;
	
	int i;
	int burstNo = 1;
	for(i = 1; i <= burstInfoSize[threadIndex - 1]; i = i + 2)
	{
		usleep(burstInfo[threadIndex - 1][i - 1]*1000);
		generationTime = getCurrentTime();
		struct burst* newBurst = malloc(sizeof(struct burst));
		newBurst->threadIndex = threadIndex;
		newBurst->burstIndex = burstNo++;
		newBurst->length = burstInfo[threadIndex - 1][i];
		newBurst->generationTime = generationTime;
		
		pthread_mutex_lock(&mutex);
		//printf("Burst created.");
		//info(newBurst);
		newBurst->next = runQueue;
		runQueue = newBurst;	
		pthread_cond_signal(&condition);
		pthread_mutex_unlock(&mutex);
	}

	pthread_exit(0); 
}

void fileMode()
{
	virtualRuntimes = (double*) malloc(sizeof(double) * N);
	
	int i;
	for(i = 0; i < N; i++)
	{
		virtualRuntimes[i] = 0;
	}
	
	runQueue = NULL;
	
	burstInfo = (int**) malloc(sizeof(int*) * N); 
	burstInfoSize = (int*) malloc(sizeof(int) * N);
	burstCount = getBurstCount();
	waitingTimes = (unsigned long**) malloc(sizeof(unsigned long*) * N); 
	
	for(i = 0; i < N; i++)
	{
		waitingTimes[i] = (unsigned long*) malloc(sizeof(unsigned long) * (burstInfoSize[i] / 2));
	}

	pthread_attr_t attr;  	
	pthread_attr_init (&attr); 
	pthread_t tids[N];
		
	int* threadIndexPtr;
	for(i = 1; i <= N; i++)
	{
		threadIndexPtr = malloc(sizeof(int));
		*threadIndexPtr = i;
		pthread_create (&tids[i - 1], &attr, generateBurstFileMode, threadIndexPtr); 
	}

	while(burstCountSoFar != burstCount)
	{    
	        pthread_mutex_lock(&mutex);
	        
	        while(runQueue == NULL)
	        {
	        	pthread_cond_wait(&condition, &mutex);
	        	
	        }
	        if(strcmp(ALG, "FCFS") == 0)
	        {
	        	FCFS();
	        }
	        else if(strcmp(ALG, "SJF") == 0)
	        {
	        	SJF();
	        }
	        else if(strcmp(ALG, "PRIO") == 0)
	        {
	        	PRIO();
	        }
	        else if(strcmp(ALG, "VRUNTIME") == 0)
	        {
	        
	        	VRUNTIME();
	        }
	        
	        pthread_mutex_unlock(&mutex);    
	}

	for(i = 1; i <= N; i++)
	{
		pthread_join (tids[i - 1], NULL); 
	}
	
	// Statistics
	int j;
	double sum = 0;
	for(i = 0; i < N; i++)
	{
		for(j = 0; j < burstInfoSize[i] / 2; j++)
		{
			sum += waitingTimes[i][j];
		}
		
		printf("Average waiting time for thread %d: %f\n", i + 1, sum / burstInfoSize[i] / 2);
	}
	
	for(i = 0; i < N; i++)
	{
		free(burstInfo[i]);
	}
	
	for(i = 0; i < N; i++)
	{
		free(waitingTimes[i]);
	}
	
	free(burstInfo);
	free(burstInfoSize);
	free(waitingTimes);
	free(virtualRuntimes);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&condition);
}

void randomMode() // schedule <N> <Bcount> <minB> <avgB> <minA> <avgA> <ALG>
{
	virtualRuntimes = (double*) malloc(sizeof(double) * N);
	waitingTimes = (unsigned long**) malloc(sizeof(unsigned long*) * N); 
	
	int i;
	for(i = 0; i < N; i++)
	{
		waitingTimes[i] = (unsigned long*) malloc(sizeof(unsigned long) * Bcount);
	}
	
	for(i = 0; i < N; i++)
	{
		virtualRuntimes[i] = 0;
	}
	
	runQueue = NULL;
	burstCount = N * Bcount;
	
	pthread_attr_t attr;  	
	pthread_attr_init (&attr); 
	pthread_t tids[N];
		
	int* threadIndexPtr;
	for(i = 1; i <= N; i++)
	{
		threadIndexPtr = malloc(sizeof(int));
		*threadIndexPtr = i;
		pthread_create (&tids[i - 1], &attr, generateBurstRandomMode, threadIndexPtr); 
	}

	while(burstCountSoFar != burstCount)
	{    
	        pthread_mutex_lock(&mutex);
	        
	        while(runQueue == NULL)
	        {
	        	pthread_cond_wait(&condition, &mutex);
	        }
	        
	        if(strcmp(ALG, "FCFS") == 0)
	        {
	        	FCFS();
	        }
	        else if(strcmp(ALG, "SJF") == 0)
	        {
	        	SJF();
	        }
	        else if(strcmp(ALG, "PRIO") == 0)
	        {
	        	PRIO();
	        }
	        else if(strcmp(ALG, "VRUNTIME") == 0)
	        {
	        	VRUNTIME();
	        }
	        
	        pthread_mutex_unlock(&mutex);    
	}

	for(i = 1; i <= N; i++)
	{
		pthread_join (tids[i - 1], NULL); 
	}
	
	// Statistics
	int j;
	double sum = 0;
	for(i = 0; i < N; i++)
	{
		for(j = 0; j < Bcount; j++)
		{
			sum += waitingTimes[i][j];
		}
		
		printf("Average waiting time for thread %d: %f\n", i + 1, sum / Bcount);
	}
	
	for(i = 0; i < N; i++)
	{
		free(waitingTimes[i]);
	}

	free(waitingTimes);
	
	free(virtualRuntimes);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&condition);
}



void VRUNTIME()
{
	if(runQueue == NULL)
	{
		return;
	}
		
	if(runQueue->next == NULL)
	{
		waitingTimes[runQueue->threadIndex - 1][runQueue->burstIndex - 1] = getCurrentTime() - runQueue->generationTime;
		//printf("Sleep for: %f\n", runQueue->length);
		usleep(runQueue->length*1000);
		//info(runQueue);
		virtualRuntimes[runQueue->threadIndex - 1] += (runQueue->length) * (0.7 + 0.3 * runQueue->threadIndex);
		burstCountSoFar++;
		free(runQueue);
		runQueue = NULL;
		
		return;
	}
	
	struct burst* burst = runQueue;
	struct burst* bursts[N];
	
	int i;
	for(i = 0; i < N; i++)
	{
		bursts[i] = NULL;
	}
	
	while(burst != NULL)
	{
		if(bursts[burst->threadIndex - 1] == NULL)
		{
			bursts[burst->threadIndex - 1] = burst;
		}
		else if(bursts[burst->threadIndex - 1]->burstIndex > burst->burstIndex)
		{
			bursts[burst->threadIndex - 1] = burst;
		}
		
		burst = burst->next;
	}
	
	burst = bursts[0];
	
	for(i = 1 ; i < N; i++)
	{
		if(burst == NULL)
		{
			burst = bursts[i];
		}
		
		if(virtualRuntimes[burst->threadIndex - 1] > virtualRuntimes[bursts[i]->threadIndex - 1])
		{
			burst = bursts[i];
		}
	}

	if(burst == runQueue)
	{
		waitingTimes[burst->threadIndex - 1][burst->burstIndex - 1] = getCurrentTime() - burst->generationTime;
		//printf("Sleep for: %f\n", burst->length);
		usleep(burst->length*1000);
		//info(burst);	
		virtualRuntimes[burst->threadIndex - 1] += burst->length * (0.7 + 0.3 * burst->threadIndex);
		burstCountSoFar++;
		runQueue = runQueue->next;
		free(burst);
		return;
	}
	
	struct burst* prev = runQueue;
	
	while(prev->next != burst)
	{
		prev = prev->next;
	}

	waitingTimes[burst->threadIndex - 1][burst->burstIndex - 1] = getCurrentTime() - burst->generationTime;
	//printf("Sleep for: %f\n", burst->length);
	usleep(burst->length*1000);
	//info(burst);
	virtualRuntimes[burst->threadIndex - 1] += burst->length * (0.7 + 0.3 * burst->threadIndex);
	burstCountSoFar++;
	
	if(burst->next == NULL)
	{
		prev->next = NULL;
		free(burst);
	}	
	else
	{
		prev->next = prev->next->next;
		free(burst);
	}	
}

void PRIO()
{
	if(runQueue == NULL)
	{
		return;
	}
		
	if(runQueue->next == NULL)
	{
		waitingTimes[runQueue->threadIndex - 1][runQueue->burstIndex - 1] = getCurrentTime() - runQueue->generationTime;
		//printf("Sleep for: %f\n", runQueue->length);
		usleep(runQueue->length*1000);
		//info(runQueue);
		burstCountSoFar++;
		free(runQueue);
		runQueue = NULL;
		
		return;
	}
	
	struct burst* burst = runQueue;
	struct burst* bursts[N];
	
	int i;
	for(i = 0; i < N; i++)
	{
		bursts[i] = NULL;
	}
	
	while(burst != NULL)
	{
		if(bursts[burst->threadIndex - 1] == NULL)
		{
			bursts[burst->threadIndex - 1] = burst;
		}
		else if(bursts[burst->threadIndex - 1]->burstIndex > burst->burstIndex)
		{
			bursts[burst->threadIndex - 1] = burst;
		}
		
		burst = burst->next;
	}
	
	burst = bursts[0];
	
	for(i = 1 ; i < N; i++)
	{
		if(burst == NULL)
		{
			burst = bursts[i];
		}
		
		if(burst->threadIndex > bursts[i]->threadIndex)
		{
			burst = bursts[i];
		}
	}

	if(burst == runQueue)
	{
		waitingTimes[burst->threadIndex - 1][burst->burstIndex - 1] = getCurrentTime() - burst->generationTime;
		//printf("Sleep for: %f\n", burst->length);
		usleep(burst->length*1000);
		//info(burst);	
		burstCountSoFar++;
		runQueue = runQueue->next;
		free(burst);
		return;
	}
	
	struct burst* prev = runQueue;
	
	while(prev->next != burst)
	{
		prev = prev->next;
	}
		
	waitingTimes[burst->threadIndex - 1][burst->burstIndex - 1] = getCurrentTime() - burst->generationTime;
	//printf("Sleep for: %f\n", burst->length);
	usleep(burst->length*1000);
	//info(burst);		
	burstCountSoFar++;
	
	if(burst->next == NULL)
	{
		prev->next = NULL;
		free(burst);
	}	
	else
	{
		prev->next = prev->next->next;
		free(burst);
	}
}

void SJF()
{
	if(runQueue == NULL)
	{
		return;
	}
		
	if(runQueue->next == NULL)
	{
		waitingTimes[runQueue->threadIndex - 1][runQueue->burstIndex - 1] = getCurrentTime() - runQueue->generationTime;
		//printf("Sleep for: %f\n", runQueue->length);
		usleep(runQueue->length*1000);
		//info(runQueue);
		burstCountSoFar++;
		free(runQueue);
		runQueue = NULL;
		
		return;
	}
	
	struct burst* burst = runQueue;
	struct burst* bursts[N];
	
	int i;
	for(i = 0; i < N; i++)
	{
		bursts[i] = NULL;
	}
	
	while(burst != NULL)
	{
		if(bursts[burst->threadIndex - 1] == NULL)
		{
			bursts[burst->threadIndex - 1] = burst;
		}
		else if(bursts[burst->threadIndex - 1]->burstIndex > burst->burstIndex)
		{
			bursts[burst->threadIndex - 1] = burst;
		}
		
		burst = burst->next;
	}
	
	burst = bursts[0];
	
	for(i = 1 ; i < N; i++)
	{
		if(burst == NULL)
		{
			burst = bursts[i];
		}
		
		if(burst->length > bursts[i]->length)
		{
			burst = bursts[i];
		}
	}

	if(burst == runQueue)
	{
		waitingTimes[burst->threadIndex - 1][burst->burstIndex - 1] = getCurrentTime() - burst->generationTime;
		//printf("Sleep for: %f\n", burst->length);
		usleep(burst->length*1000);
		//info(burst);	
		burstCountSoFar++;
		runQueue = runQueue->next;
		free(burst);
		return;
	}
	
	struct burst* prev = runQueue;
	
	while(prev->next != burst)
	{
		prev = prev->next;
	}
	
	waitingTimes[burst->threadIndex - 1][burst->burstIndex - 1] = getCurrentTime() - burst->generationTime;
	//printf("Sleep for: %f\n", burst->length);	
	usleep(burst->length*1000);
	//info(burst);	
	burstCountSoFar++;
	
	if(burst->next == NULL)
	{
		prev->next = NULL;
		free(burst);
	}	
	else
	{
		prev->next = prev->next->next;
		free(burst);
	}
}

void FCFS()
{	
	if(runQueue == NULL)
	{
		return;
	}
	
	if(runQueue->next == NULL)
	{
		waitingTimes[runQueue->threadIndex - 1][runQueue->burstIndex - 1] = getCurrentTime() - runQueue->generationTime;
		//printf("Sleep for: %f\n", runQueue->length);
		usleep(runQueue->length*1000);
		//info(runQueue);
		burstCountSoFar++;
		free(runQueue);
		runQueue = NULL;
		
		return;
	}
	
	struct burst* burst = runQueue;
	
	while(burst->next->next != NULL)
	{
		burst = burst->next;
	}
	
	waitingTimes[burst->next->threadIndex - 1][burst->next->burstIndex - 1] = getCurrentTime() - burst->next->generationTime;
	//printf("Sleep for: %f\n", burst->next->length);
	usleep(burst->next->length*1000);
	//info(burst->next);
	burstCountSoFar++;
	free(burst->next);
	burst->next = NULL;	
}

unsigned long getCurrentTime()
{
	struct timeval timeValue;
	gettimeofday(&timeValue, NULL);
	unsigned long currentTime = timeValue.tv_usec;
	currentTime = currentTime + timeValue.tv_sec * 1e6;
	currentTime = currentTime / 1000;
	return currentTime;
}

int isDigit(char c)
{
	if(c == '0' || c == '1' || c == '2' || c == '3' || c == '4' 
	|| c == '5' || c == '6' || c == '7' || c == '8' || c == '9')
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int getBurstCount()
{
	int burstCount = 0;
	int index;
	
	for(index = 1; index <= N; index++)
	{
		char file[256];
		char indexStr[10]; 
		itoa(index,indexStr);
		strcpy(file, fileName);
		strncat(file, "-", 2);
		strncat(file, indexStr, 2);
		strncat(file, ".txt", 5);
		
		char c;
		char str[1024];
		int count = 0;
		
		int fd1 = open(file, O_RDONLY);
		
		while (read(fd1, &c, 1) == 1)
		{
			if(isDigit(c) == 1)
			{
				strncat(str, &c, 1);

			}
			else if(strcmp(str, "") != 0 )
			{
				count++;	
				strcpy(str, "");
			}
		}	
		
		close(fd1);
		
		if(strcmp(str, "") != 0 )
		{
			count++;	
			strcpy(str, "");
		}
		
		burstCount = burstCount + count / 2;
		
		fd1 = open(file, O_RDONLY);
		int* numbers = (int*) malloc(sizeof(int) * count);
		burstInfoSize[index - 1]  = count;
		int current = 0;
		
		
		while (read(fd1, &c, 1) == 1)
		{
			if(isDigit(c) == 1)
			{
				strncat(str, &c, 1);

			}
			else if(strcmp(str, "") != 0 )
			{
				numbers[current] = atoi(str);
				current++;	
				strcpy(str, "");
			}
		}	
		
		close(fd1);
		burstInfo[index - 1] = numbers;
	}
	
	return burstCount;
}

void swap(char *x, char *y) 
{
    char t = *x; *x = *y; *y = t;
}

char* reverse(char *buffer, int i, int j)
{
    while (i < j)
    {
   	 swap(&buffer[i++], &buffer[j--]);
    }
        
    return buffer;
}

char* itoa(int value, char* buffer)
{
    int n = abs(value);
 
    int i = 0;
    while (n)
    {
        int r = n % 10;
 
        if (r >= 10) 
        {
            buffer[i++] = 65 + (r - 10);
        }
        else
        {
            buffer[i++] = 48 + r;
 	}
 
        n = n / 10;
    }
 
    buffer[i] = '\0'; 
 
    return reverse(buffer, 0, i - 1);
}

void info(struct burst* burst)
{
	printf("t: %d, b: %d, l: %f, g: %lu\n", burst->threadIndex, burst->burstIndex, burst->length, burst->generationTime);
}

double getRandomA(double avg)
{
	double number = 0;
	
	while(number < minA)
	{
		double lambda = 1/avg;
		double u = rand() / (RAND_MAX + 1.0);
		number = -log(1- u) / lambda;
	}
	
	return number;
}

double getRandomB(double avg)
{
	double number = 0;
	
	while(number < minB)
	{
		double lambda = 1/avg;
		double u = rand() / (RAND_MAX + 1.0);
		number = -log(1- u) / lambda;
	}
	
	return number;
}

