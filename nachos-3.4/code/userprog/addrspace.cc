// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#include <stdio.h>

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

extern int swapMode;
static Semaphore invPageTableSemaphore("Inverted Page Table", 1);

static struct {
	int time;
	int process;
	Thread* processThread;
	int page;
} invPageTable[32];

AddrSpace::AddrSpace(OpenFile *executable)
{
	static int invPageTableNotYetLoaded = 1;
	if(invPageTableNotYetLoaded) {
		
		invPageTableSemaphore.P();

		for(int i = 0; i < 32; i++) {
			invPageTable[i].time = 0;
			invPageTable[i].process = -1;
			invPageTable[i].page = -1;
			invPageTable[i].processThread = NULL;
		}

		invPageTableNotYetLoaded = 0;
		
		invPageTableSemaphore.V();
	}

	pageTable = NULL;

    NoffHeader noffH;
    unsigned int i, size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;

    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

	// first, set up the translation
    pageTable = new TranslationEntry[numPages];
    for (i = 0; i < numPages; i++) {
		pageTable[i].virtualPage = i;
		//pageTable[i].physicalPage = i + startPage;
		pageTable[i].valid = FALSE;
		pageTable[i].use = FALSE;
		pageTable[i].dirty = FALSE;
		pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
						// a separate page, we could set its 
						// pages to be read-only
    }

}
 




//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

//Because the initialization already zeroes out the memory to be used,
//is it even necessary to clear out any garbage data during deallocation?

AddrSpace::~AddrSpace()
{
	if(pageTable != NULL){
		for(unsigned i = 0; i < numPages; i++) {
			if(pageTable[i].valid == 1) {
				memMap->Clear(pageTable[i].physicalPage);
				invPageTable[pageTable[i].physicalPage].processThread = NULL;
				invPageTable[pageTable[i].physicalPage].time = 0;
				invPageTable[pageTable[i].physicalPage].process = -1;
				invPageTable[pageTable[i].physicalPage].page = -1;
				
			}
			pageTable[i].valid = 0;
			pageTable[i].dirty = 0;
		}
		delete pageTable, pageTable = 0;
		memMap->Print();
	}
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

void AddrSpace::GenerateSWAP(OpenFile *executable, int ID)  {


    NoffHeader noffH;
    unsigned int size;

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;

    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;

	char filename[100];
	sprintf(filename, "%d.swap", ID);
	//printf("\nfileSystem->Create(\"%s\")", filename);
	fileSystem->Create(filename, size);
	OpenFile* swapFile = fileSystem->Open(filename);
	
	char swapData[size];

	bzero((void*)swapData, size);

	if (noffH.code.size > 0)
        executable->ReadAt(&swapData[noffH.code.virtualAddr], noffH.code.size, noffH.code.inFileAddr);

	if (noffH.initData.size > 0)
		executable->ReadAt(&swapData[noffH.initData.virtualAddr], noffH.initData.size, noffH.initData.inFileAddr);

	//printf("SWAPFILE\n");

	//for(int i = 0; i < size; i++) printf("%x", swapData[i]);

	swapFile->Write(swapData, size);

	delete swapFile;

	/*printf("READFILE\n");

	OpenFile* readFile = fileSystem->Open("1.swap");
	char readData[readFile->Length()];

	readFile->Read(readData, readFile->Length());
	
	for(int i = 0; i < size; i++) printf("%x", readData[i]);

	delete readFile;*/
/*
// zero out the entire address space, to zero the unitialized data segment
// and the stack segment
    //bzero(machine->mainMemory, size); rm for Solaris
	//Edited version adds startPage * PageSize to the address. Hopefully this is proper.
	//Either way, it appears to zero out only however much memory is needed,
	//so zeroing out all memory doesn't seem to be an issue. - Devin

	pAddr = startPage * PageSize;

    memset(machine->mainMemory + pAddr, 0, size);
//then, copy in the code and data segments into memory
//Change these too since they assume virtual page = physical page
	  //Fix this by adding startPage times page size as an offset
    if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr + (startPage * PageSize), noffH.code.size);
        executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr + pAddr]),
			noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr + (startPage * PageSize), noffH.initData.size);
        executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr + pAddr]),
			noffH.initData.size, noffH.initData.inFileAddr);
    }

	*/

}







int FrameSearch() {
	int finalAns = -1;
	int O = 0;
	int ans = -1;
	//invPageTableSemaphore.P();
	switch(swapMode) {
		case 0:
			printf("\nBorking NachOS by Process %d\n", currentThread->getID());
			break;
		case 1:
	
			for(int i = 0; i < 32; i++) {
				if(invPageTable[i].time > O) {
					O = invPageTable[i].time;
					ans = i;
				}
			}
			finalAns = ans;

			break;
		case 2:
			finalAns = Random() % 32;
			break;
	}
	//invPageTableSemaphore.V();
	return finalAns;
}






void AddrSpace::KillSWAP(int theThreadID)
{
	char filename[100];
	sprintf(filename, "%d.swap", theThreadID);

	

	//printf("\nfileSystem->Remove(\"%s\")", filename);
	fileSystem->Remove(filename);
}










int AddrSpace::PageFaultLoadPage(int pageFaultAddr, int theThreadID) {
	invPageTableSemaphore.P();
	memMap->Print();
	int page = pageFaultAddr / PageSize;
	int pageOffset = page * PageSize;

	int frame = memMap->Find();
	int frameOffset = frame * PageSize;
		
	//printf("\tpage: %d\tframe: %d\tPageSize: %d\tframeOffset: %d\t", page, frame, PageSize, frameOffset);

	if(-1 == frame)
	{
		printf("\nNo open Frames\n");
		frame = FrameSearch();
		//printf("frame: %d, process: %d, %d\n", frame, invPageTable[frame].process, invPageTable[frame].processThread->getID());
		
		if(-1 != frame) {
			
			frameOffset = frame * PageSize;

			int physicalOffset = frame * PageSize;
			int virtualOffset = invPageTable[frame].page * PageSize;
			
			char filename[100];
			sprintf(filename, "%d.swap", invPageTable[frame].process);
			
			OpenFile* writeFile = fileSystem->Open(filename);
			
			char writeData[PageSize];
			
			for(int i = 0; i < PageSize; i++)
				writeData[i] = machine->mainMemory[physicalOffset + i];
			
			writeFile->WriteAt(writeData, PageSize, virtualOffset);

			invPageTable[frame].processThread->space->pageTable[invPageTable[frame].page].valid = FALSE;

			delete writeFile;

		}
	}
	if(-1 == frame) {
		return 1;
	}
	//empty frame found
	char filename[100];
	sprintf(filename, "%d.swap", theThreadID);


	//printf("\nfileSystem->Open(\"%s\")", filename);

	OpenFile* readFile = fileSystem->Open(filename);
	char readData[PageSize];

	readFile->ReadAt(readData, PageSize, pageOffset);
	
	//printf("\nREADFILE:\n");
	//for(int i = 0; i < PageSize; i++) printf("%x", readData[i]);

	delete readFile;
	//printf("frameOffset=%d.", frameOffset);
	for(int i = 0; i < PageSize; i++)
		machine->mainMemory[frameOffset + i] = readData[i];

	
	pageTable[page].valid = TRUE;
	pageTable[page].virtualPage = page;
	pageTable[page].physicalPage = frame;
		
	for(int i = 0; i < 32; i++)
		invPageTable[i].time++;
	invPageTable[frame].time = 0;
	invPageTable[frame].process = theThreadID;
	invPageTable[frame].processThread = currentThread;
	invPageTable[frame].page = page;
		
	memMap->Print();
	invPageTableSemaphore.V();

	return 0;
	
}	

