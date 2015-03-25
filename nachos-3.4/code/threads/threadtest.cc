// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustrate the inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synch.h"
#include <ctype.h>

#define HIDE 0
#define RUN 1
#define ZOMBIE 2

#define NOTTAKEN 0
#define TAKEN 1

enum rockBand {READYTOPLAY, NOTREADY, SPECTATOR, WANTING};

int * humans;
rockBand * players;
int * instruments;
int songsPlayed;
int numPlayers;
int numSongs;
int numIns;
int numSpecs;
Semaphore * getInstrument;
bool specFlag;
bool songFlag;
bool incrementFlag;
bool arrivalFlag;
int numReady;
int leaving;
bool leavingFlag;

bool zombieFlag;

int numShouters;
int numShouts;

int chance;
int maxThread;

int numPhilos;
int philoSat;
int numMeals;
int philoAte;
Semaphore ** chopsticks;
bool * chops;
//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
PhiloSema(int phID)
{
	int left = phID;
	int right = phID+1;
	if(right >= numPhilos)
		right = 0;

	printf("Philosoraptor %d has staggered in.\n", phID);
	++philoSat;
	while(philoSat < numPhilos)
		currentThread->Yield();

	if(philoSat == numPhilos)
	{
		printf("All philosoraptors are here, surprisingly.\n\n");
		++philoSat;
	}

	int timesToLoop = Random()%5 + 1;

	while(philoAte < numMeals)
	{
		chopsticks[left]->P();
		printf("   Philosoraptor %d has picked up his left chopstick(#%d).\n", phID, left);
		chopsticks[right]->P();
		printf("   Philosoraptor %d has picked up his right chopstick(#%d).\n", phID, right);

		if(philoAte >= numMeals)
			printf("   But there are no more meals.  Sad day. :(\n");
		else {
			++philoAte;
			printf("     %d Philosoraptor(s) has/have nommed so far.\n", philoAte);

			while(timesToLoop > 0)
			{
				timesToLoop--;
				currentThread->Yield();
			}

			timesToLoop = Random()%5 + 1;
			}// end else

		chopsticks[left]->V();
		printf("   Philosoraptor %d has put down his left chopstick(#%d).\n", phID, left);
		chopsticks[right]->V();
		printf("   Philosoraptor %d has put down his right chopstick(#%d).\n", phID, right);

		while(timesToLoop > 0)
		{
			printf("Philosoraptor %d is thinking, oddly enough.\n", phID);
			--timesToLoop;
			currentThread->Yield();
		}
		timesToLoop = Random()%5 + 1;
	}// end big while

	printf("Philosoraptor %d runs off, stalking new prey.\n", phID);
}

void
PhiloBusy(int phID)
{
	int left = phID;
	int right = phID+1;
	if(right >= numPhilos)
		right = 0;

	printf("Philosoraptor %d has staggered in.\n", phID);
	++philoSat;
	while(philoSat < numPhilos)
		currentThread->Yield();

	if(philoSat == numPhilos)
	{
		printf("All philosoraptors are here, surprisingly.\n\n");
		++philoSat;
	}

	int timesToLoop = Random()%5 + 1;

	while(philoAte < numMeals)
	{
		while(!chops[left])
			currentThread->Yield();
		chops[left] = false;
		printf("   Philosoraptor %d has picked up his left chopstick(#%d).\n", phID, left);
		while(!chops[right])
			currentThread->Yield();
		chops[right] = false;
		printf("   Philosoraptor %d has picked up his right chopstick(#%d).\n", phID, right);

		if(philoAte >= numMeals)
			printf("   But there are no more meals.  Sad day. :(\n");
		else {
			++philoAte;
			printf("     %d Philosoraptor(s) has/have nommed so far.\n", philoAte);

			while(timesToLoop > 0)
			{
				timesToLoop--;
				currentThread->Yield();
			}

			timesToLoop = Random()%5 + 1;
			}// end else

		chops[left] = true;
		printf("   Philosoraptor %d has put down his left chopstick(#%d).\n", phID, left);
		chops[right] = true;
		printf("   Philosoraptor %d has put down his right chopstick(#%d).\n", phID, right);

		while(timesToLoop > 0)
		{
			printf("Philosoraptor %d is thinking, oddly enough.\n", phID);
			--timesToLoop;
			currentThread->Yield();
		}
		timesToLoop = Random()%5 + 1;
	}// end big while

	printf("Philosoraptor %d runs off, stalking new prey.\n", phID);
}

bool
zombieCheck()
{
	for(int i = 0; i < 10; i++){
		if(humans[i]!=2)
			return true;
	}
	if(!zombieFlag){
		printf("All humans are now zombies!  Curses!\n");
		zombieFlag=true;
	}
	return false;
}

void
zombieStuff(int which)
{
	while(zombieCheck())
	{
		int omnomnom = Random()%10;
		if(humans[omnomnom]==RUN)
		{
			printf("Zombie %i has captured human %i!  Uh oh!\n",which,omnomnom);
			humans[omnomnom]=ZOMBIE;
			printf("Human %i was turned into a zombie!\n",omnomnom);
		}
		else
			printf("Zombie %i is shambling around!\n",which);
		currentThread->Yield();
	}
}

void
humanStuff(int which)
{
	while(true){
		if(humans[which]!=ZOMBIE){
			int choice = Random()%2;
			int waitTime = Random()%5;
			while(waitTime < 2)
				waitTime=Random()%5;
			
			humans[which]=choice;
			
			if (choice==HIDE)
			{
				printf("Human %i is hiding!\n",which);
				while(waitTime>0){
					waitTime--;
					currentThread->Yield();
					}
			}
			else if (choice==RUN)
			{
				printf("Human %i got spooked and is running in sheer bloody panic!\n",which);
				currentThread->Yield();
			}
		}
		else
		{
			zombieStuff(which);
			currentThread->Finish();
		}
	}
}

void
shoutingThreads(int threadNum)
{
	int timesShouted = 0;
	int toShout = 0;
	while(timesShouted < numShouts)
	{
		printf("Thread %i yells: ", threadNum);
		toShout = Random()%4;
		if(toShout == 0)
			printf("This is the ultimate!\n");
		else if(toShout == 1)
			printf("CHAOS CONTROL!\n");
		else if(toShout == 2)
			printf("Maria...\n");
		else if(toShout == 3)
			printf("Yosh!\n");
		
		timesShouted++;
		toShout = Random()%5+1;
		while(toShout > 0) {
			currentThread->Yield();
			toShout--;
		}
	}
}

int
getNumber()
{
	int returnThis = 0;
	char* toTest = new char;
	while(returnThis<1){
		*toTest = getchar();
		while(*toTest != '\n') {
			returnThis *= 10;
			if(isdigit(*toTest)) {
				returnThis += atoi(toTest);
				*toTest = getchar();
			} else {
				printf("Bad input.  Try again.");
				return -1;
			}
		}
		if(returnThis > 1000 || returnThis < 0) {
			printf("Bad input.  Please enter a number between 1 and 999.\n");
			returnThis = 0;
			*toTest = '!';
		}
	}
	return returnThis;
}

void
spawn(int parent)
{
	int randomized = Random()%99;
	Thread *newThread;
	while(randomized <= chance)
	{
		maxThread++;
		printf("Thread %i has been born to Thread %i\n", maxThread, parent);
		newThread = new Thread("thing");
		newThread->Fork(spawn,maxThread);
		randomized = Random()%99;
		currentThread->Yield();
	}
	printf("Thread %i has stopped having children.\n", parent);
}

bool RockCheck()
{
	for(int i = 0; i < numPlayers; i++){
		if(players[i] == NOTREADY || players[i] == WANTING)
			return false;
		}
	return true;
}

void ReadyCheck()
{
	numReady++;
	while(numReady < numPlayers)
		currentThread->Yield();
}

void
RockBand(int threadNum)
{
	int temp = 0;
	printf("Thread %i has arrived!\n",threadNum);
	
	if(threadNum == numPlayers-1) {
		printf("All threads are here!  Let's rock!\n");
		arrivalFlag = true;
		currentThread->Yield();
	}
	
	while(!arrivalFlag)
		currentThread->Yield();
		
	while(songsPlayed < numSongs){
		if(Random()%99 < 50)
			players[threadNum] = WANTING;
		else
			players[threadNum] = SPECTATOR;
			
		if(players[threadNum] == WANTING){
			getInstrument->P();
			temp = Random()%numIns;
			if(instruments[temp] != TAKEN){
				printf("Thread %i took instrument %i!\n",threadNum,temp);
				instruments[temp] = TAKEN;
				players[threadNum] = READYTOPLAY;
				}
			else
				players[threadNum] = SPECTATOR;
			getInstrument->V();
			}
		
		while(!RockCheck())
			currentThread->Yield();
			
		if(!specFlag){
			printf("Spectators: ");
			numSpecs = 0;
			for(int i = 0; i < numPlayers; i++){
				if(players[i]==SPECTATOR)
					numSpecs++;
				}
			specFlag = true;
			}
		
		if(numSpecs == 0){
			printf("none.\n");
			songFlag = true;
			}
		
		while(!songFlag){
			if(players[threadNum] == SPECTATOR){
				if(numSpecs > 1){
					printf("%i, ",threadNum);
					numSpecs--;
					}
				else if(numSpecs == 1) {
					printf("%i!\n",threadNum);
					songFlag = true;
					}
				}
			currentThread->Yield();
			}
		
		if(!incrementFlag){
			songsPlayed++;
			printf("*** Total songs played: %i\n",songsPlayed);
			incrementFlag = true;
		}

		if(players[threadNum] == READYTOPLAY){
			getInstrument->P();
			printf("Thread %i dropped instrument %i!\n",threadNum,temp);
			instruments[temp] = NOTTAKEN;
			getInstrument->V();
			currentThread->Yield();
			players[threadNum] = NOTREADY;
			}
		else if (players[threadNum] == SPECTATOR) {
			players[threadNum] = NOTREADY;
			}
			
		ReadyCheck();
		if(numReady == numPlayers)
			currentThread->Yield();
			
		numReady = 0;
		
		incrementFlag = false;
		songFlag = false;
		specFlag = false;
		}
	
	leaving++;
	if(leaving == numPlayers){
		printf("All threads are ready to leave!\n");
		leavingFlag = true;
		}
		
	while(leaving < numPlayers && !leavingFlag)
		currentThread->Yield();
	
	printf("Thread %i has dropped the mic and left.\n",threadNum);
	leaving--;
	
	if(leaving == 0)
		printf("All threads have skedaddled.\n");
}

void
ThreadTest()
{
	if(threadChoice==1)
	{
		printf("Enter the number of shouting threads: ");
		do {
		numShouters = getNumber();
		} while (numShouters < 0);
		
		printf("Enter the number of shouts per thread: ");
		do {
		numShouts = getNumber();
		} while (numShouts < 0);
		
		Thread *t = new Thread("shouter!");
		for(int i = 0; i < numShouters; i++) {
			t = new Thread("shouter!");
			t->Fork(shoutingThreads, i);
		}
	}
	else if (threadChoice==2)
	{
		printf("Oh snap!  Zombies!  How many unlucky humans? ");
		int numHumans = getNumber();
		humans = new int[numHumans];
		
		for (int i = 0; i < numHumans; i++)
			humans[i] = HIDE;
			
		Thread *t = new Thread("zombie!");
		t->Fork(zombieStuff,numHumans+1);
		
		for(int i = 0; i < numHumans; i++){
			t = new Thread("human!");
			t->Fork(humanStuff,i);
			}
	}
	else if (threadChoice==3)
	{
		printf("Enter the spawn chance (between 1 and 99): ");
		do {
		chance = getNumber();
		} while (chance < 0 && chance <= 99);
		
		maxThread = 0;
		Thread *start = new Thread("Patient 0");
		printf("Thread 0 has been born to ***.\n");
		start->Fork(spawn, 0);
	}
	else if (threadChoice==4 || threadChoice == 5)
	{
		philoSat = 0;
		numPhilos = 0;
		numMeals = 0;
		philoAte = 0;
		printf("How many philosoraptors dining today? ");
		numPhilos = getNumber();
		printf("And how many meals? ");
		numMeals = getNumber();
		
		Thread *t = new Thread("Potato!");

		chopsticks = new Semaphore*[numPhilos];
		for(int j = 0; j < numPhilos; ++j)
			chopsticks[j] = new Semaphore((char*)'A', 1);
		if(threadChoice == 2) {
			for(int i = 0; i < numPhilos; ++i){
				t = new Thread("Philo!");
				t->Fork(PhiloSema, i);
				}
			}
		else {
			chops = new bool[numPhilos];

			for (int q = 0; q < numPhilos; q++)
				chops[q] = true;

			for(int i = 0; i < numPhilos; i++)
			{
				t = new Thread("PhiloBusy!");
				t->Fork(PhiloBusy, i);
			}
		}
	}
	else if (threadChoice == 6)
	{
		printf("Thread Rock Band!  How many players? ");
		numPlayers = getNumber();
		printf("How many instruments? ");
		numIns = getNumber();
		printf("How many songs? ");
		numSongs = getNumber();
		
		players = new rockBand[numPlayers];
		instruments = new int[numIns];
		
		for(int i = 0; i < numPlayers; i++)
			players[i] = NOTREADY;
		
		for(int j = 0; j < numIns; j++)
			instruments[j] = NOTTAKEN;
		
		songsPlayed = 0;
		specFlag = false;
		songFlag = false;
		arrivalFlag = false;
		incrementFlag = false;
		leavingFlag = false;
		numReady = 0;
		
		getInstrument = new Semaphore("Blah!",1);
		
		Thread *t = new Thread("bleh!");
		for(int k = 0; k < numPlayers; k++){
			t = new Thread("rock!");
			t->Fork(RockBand, k);
			}
	}
	else
		printf("Invalid -A option.  Try again.\n");
}
