/*The memsim.c is a memory simulator which has LRU and VMS page replacement algorithms. Here the number of read, writes, event traces are calculated. It is run for
gcc.trace, swim.trace, bzip.trace, sixpack.trace. It has two modes one is the quite mode and other is the debug mode. */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

int nFrames, CleanPageList=0;
char *mode;
double hitrate=0.00;
int Reads=0, Writes=0, Iterations=0,NumberOfMyDiskReads=0;

//For LRU algo
struct lrupageframe 
{
	unsigned int occ;  
	char vbit;  //VALID BIT
	char dbit; //DIRTY BIT
	unsigned int pnum;       
};

typedef struct vmspageframe 
{
	struct vmspageframe *next;
	char vbit; 
	char dbit;
	unsigned int pnum;       
} vmspageframe_t;

typedef struct Queue
{
    struct Queue *next;
} Queue_t;  

typedef struct QHead
{
    struct Queue *start;
    struct Queue *end;
    int Count;
} QHead_t;  


struct lrupageframe *lruTable;  //lruTable is used as pointer to array of structures.

//for vms
vmspageframe_t *vmsTable; //VMSTable is used as pointer to array of structures.
QHead_t vmsCleanPageListHead, vmsDirtyPageListHead;


//prints the final OUTPUT
void Output(void)
{
	printf("total memory frames: %d\n",nFrames);
	printf("events in trace: %d\n", Iterations);
	printf("total disk reads: %d\n",Reads); 
	printf("total disk writes: %d\n",Writes);
	if(strcmp(mode,"debug")==0)
	printf("hit rate: %.2f\n", (double)((1000000-Reads)*100/(double)1000000)); //HIT RATIO IS CALCULATED
}


//FindLeastRecentlyUsedEntry

struct lrupageframe *LeastRecentlyUsedEntry(void)
{
	int i;
	struct lrupageframe *lru_TableEntry,*lru_MaxTableEntry;

	lru_TableEntry = lru_MaxTableEntry = lruTable;
	i=0;
    while(i<nFrames)
	{
		if(lru_TableEntry->occ > lru_MaxTableEntry->occ)
			lru_MaxTableEntry = lru_TableEntry;
		lru_TableEntry++;
		i++;
	}
	return(lru_MaxTableEntry);
}	


//Update Reference Variable for lru

void UpdateRefVar(unsigned int pnum, char rw)
{
	int i;
	struct lrupageframe *lru_TableEntry;
	i=0;
	lru_TableEntry = lruTable;
	while(i<nFrames)
	{
		if((lru_TableEntry->pnum != pnum) && (lru_TableEntry->vbit))
			lru_TableEntry->occ++;      
		if((rw == 'W') && (lru_TableEntry->pnum ==  pnum))
			lru_TableEntry->dbit = 'W';
		lru_TableEntry++;
		i++;
	}	
}


void InitT(void)
{
	int i;
	struct lrupageframe *lru_TableEntry;
	i=0;
	lru_TableEntry = lruTable;
	while(i<nFrames)
	{
		lru_TableEntry->dbit = 'R';//intializing all page frames to read type.
		lru_TableEntry++;
		i++;
	}	
}


//for vms

void InitTableLink(void)
{
	int i;
	vmspageframe_t *vms_EntryLink;

    CleanPageList=0;
	vmsCleanPageListHead.start = NULL;
	vmsCleanPageListHead.end = NULL;	
	vmsCleanPageListHead.Count = 0;
	vmsDirtyPageListHead.start = NULL;
	vmsDirtyPageListHead.end = NULL;	
	vmsDirtyPageListHead.Count = 0;
   
  i=0;
  vms_EntryLink = vmsTable;  
	while(i<nFrames)
	{
		vms_EntryLink->dbit = 'R';//intializing all page frames to read type.
		vms_EntryLink++;
		i++;
	}	
}

vmspageframe_t *FreeLocInRSS(void) //to find free location in RSS
{
	int i;
	vmspageframe_t *vms_EntryLink;
    i=0;
    vms_EntryLink = vmsTable;
    while(i< nFrames)
    {
        if(!vms_EntryLink->vbit)
            return vms_EntryLink;
        i++;
        vms_EntryLink++;
    }
    return NULL;
}


unsigned int GetLog_PageNum(unsigned int addr) //Get logical page number
{
    return ((addr>>12));
}


Queue_t *RemEleFromQueue(QHead_t *QueueHead, Queue_t *pEle)
{
    Queue_t *pE,*pENext;
    
    if(!QueueHead->Count)
        return NULL;           
    if(QueueHead->Count == 1)   // if only one element is present in the queue
    {
        if(QueueHead->start == pEle)
        {
            QueueHead->start = NULL;
            QueueHead->end = NULL;
            QueueHead->Count = 0;
            return pEle;
        }
        else
            return NULL;  //if no element is present.
    }
    pE = QueueHead->start;
    if(pE == pEle)  //if first element
    {
        QueueHead->start = pEle->next;
        QueueHead->Count--;
        return pEle;
    }
    
    while(pE->next != pEle)
    {
        pE = pE->next;
    }
    pE->next = pEle->next;
    if(QueueHead->end == pEle)
        QueueHead->end = pE;
    QueueHead->Count--;
    return pEle;
}



int GetQLen(QHead_t *QueueHead)
{
    return QueueHead->Count;
}


int PutInStartQ(QHead_t *QueueHead, Queue_t *pEle)
{
    if(!QueueHead->Count)
    {
         pEle->next = NULL;
         QueueHead->start = QueueHead->end = pEle;
         QueueHead->Count = 1;
    }
    else
    {
         pEle->next = QueueHead->start;	
         QueueHead->start = pEle;
         QueueHead->Count++;
    }
    return 0;
}
Queue_t *GetFromLast(QHead_t *QueueHead)
{
    Queue_t *pEle, *pTempEle, *pTemp1Element;
    
    if(!QueueHead->Count)
        return NULL;
    else
    {
        pEle = QueueHead->start;
        if(QueueHead->Count == 1)
        {
            QueueHead->start = QueueHead->end = NULL;
            QueueHead->Count--;
            return pEle;
        }
        else if(QueueHead->Count == 2)
        {
            pEle = QueueHead->end;
            QueueHead->end = QueueHead->start;
            QueueHead->start->next=NULL;
            QueueHead->Count--;
            return pEle;
        }
        else
        {
            pTempEle = QueueHead->start->next;
            while((pTempEle->next != QueueHead->end) &&
                    (pTempEle->next->next != NULL))
            {
                pTempEle = pTempEle->next;
            }
            pEle = QueueHead->end;
            QueueHead->end = pTempEle;
            pTempEle->next = NULL;
            QueueHead->Count--;
            return pEle;            
        }
    }
}

//vms
void vms(unsigned int addr, char rw)
{
	int i,pnum,efound=0;
	vmspageframe_t *vms_EntryLink, *vms_TempEntryLink;
	
	pnum = GetLog_PageNum(addr);
	
    //Before Half the resident set size is exausted.
    if(CleanPageList <=(nFrames/2))
    {
        //if page already present in pageFrame Memory and then check only the clean list
        if(!(vms_EntryLink = (vmspageframe_t *)vmsCleanPageListHead.start))
        {   //is clean list to be empty
            vms_EntryLink = vmsTable;
            vms_EntryLink->vbit = 1;
            vms_EntryLink->pnum = pnum;
            vms_EntryLink->dbit = rw;
            PutInStartQ((QHead_t *)&vmsCleanPageListHead, (Queue_t *)vms_EntryLink); // put it in CleanPageListQ
            CleanPageList++;
            Reads++;            
            return;
        }
        vms_EntryLink = (vmspageframe_t *)vmsCleanPageListHead.start;
        while(vms_EntryLink)
        {  
      
            if((vms_EntryLink->pnum == pnum) && (vms_EntryLink->vbit))
            {
                efound = 1;
                break;
            }
            vms_EntryLink = vms_EntryLink->next;
        }
        if(efound)
        {
        	if(strcmp(mode,"debug")==0)
            	printf("Page found in clean list\n");
            if((vms_EntryLink->dbit == 'R') && (rw == 'W'))
                vms_EntryLink->dbit = 'W';   //update dirty bit 
            return;
        }
        else
        {
            i=0;
            vms_EntryLink = vmsTable;
            while(i<=(nFrames/2))
            {
                if(!vms_EntryLink->vbit)
                {
                    vms_EntryLink->vbit = 1;
                    vms_EntryLink->pnum = pnum;
                    vms_EntryLink->dbit = 'R';
                    PutInStartQ((QHead_t *)&vmsCleanPageListHead, (Queue_t *)vms_EntryLink); // put it in CleanPageListQ
                    CleanPageList++;                    
                    Reads++;                    
                    return;
                }
                i++;
                vms_EntryLink++;
            }
        }
	}
//--------------------------------------------------------------------------------------------
    else
    {
        //half the RSS is consumed.
        if(CleanPageList != nFrames)
        {
            //if RSS memory is not consumed
            vms_EntryLink = (vmspageframe_t *)vmsCleanPageListHead.start;
            while(vms_EntryLink)
            {   
                
                if((vms_EntryLink->pnum == pnum) && (vms_EntryLink->vbit))
                {
                    efound = 1;
                    break;
                }
                vms_EntryLink = vms_EntryLink->next;
            }
            if(efound)
            {
            	if(strcmp(mode,"debug")==0)
                	printf("Page found in Clean list\n");
                if((vms_EntryLink->dbit == 'R') && (rw == 'W'))
                    vms_EntryLink->dbit = 'W';   //update Dirty bit
                return;
            }
            else
            {
                if(!(vms_EntryLink = (vmspageframe_t *)vmsDirtyPageListHead.start))
                {   //if Dirtylist is empty  
                    vms_EntryLink = FreeLocInRSS();
                    vms_EntryLink->vbit = 1;
                    vms_EntryLink->pnum = pnum;
                    vms_EntryLink->dbit = 'R';
                    vms_TempEntryLink = (vmspageframe_t *)GetFromLast((QHead_t *)&vmsCleanPageListHead);
                    PutInStartQ((QHead_t *)&vmsCleanPageListHead, (Queue_t *)vms_EntryLink); // put it in Clean page list
                    PutInStartQ((QHead_t *)&vmsDirtyPageListHead, (Queue_t *)vms_TempEntryLink); // put it in Dirty Page list                        
                    CleanPageList++;
                    Reads++;                    
                    return;
                }
                i=0;
                vms_EntryLink = (vmspageframe_t *)vmsDirtyPageListHead.start;
                while(vms_EntryLink)
                {     
                    if((vms_EntryLink->pnum == pnum) && (vms_EntryLink->vbit))
                    {
                        efound = 1;
                        break;
                    }
                    vms_EntryLink = vms_EntryLink->next;
                }
                if(efound)
                {
                    if(strcmp(mode,"debug")==0)
                    	printf("Page found in Dirty list\n");
                    RemEleFromQueue((QHead_t *)&vmsDirtyPageListHead, (Queue_t *)vms_EntryLink);    
                    if((vms_EntryLink->dbit == 'R') && (rw == 'W'))
                        vms_EntryLink->dbit = 'W'; 
                    vms_TempEntryLink = (vmspageframe_t *)GetFromLast((QHead_t *)&vmsCleanPageListHead);
                    PutInStartQ((QHead_t *)&vmsDirtyPageListHead, (Queue_t *)vms_TempEntryLink);
                    PutInStartQ((QHead_t *)&vmsCleanPageListHead, (Queue_t *)vms_EntryLink);               
                    return;
                }
                else
                {
                    vms_EntryLink = FreeLocInRSS();
                    vms_EntryLink->vbit = 1;
                    vms_EntryLink->pnum = pnum;
                    vms_EntryLink->dbit = 'R';
                    vms_TempEntryLink = (vmspageframe_t *)GetFromLast((QHead_t *)&vmsCleanPageListHead);
                    PutInStartQ((QHead_t *)&vmsCleanPageListHead, (Queue_t *)vms_EntryLink);
                    PutInStartQ((QHead_t *)&vmsDirtyPageListHead, (Queue_t *)vms_TempEntryLink);                     
                    CleanPageList++;
                    Reads++;                   
                    return;
                }
            }
         }
         else
         {
             
             //RSS memory is consumed
            i=0;
            vms_EntryLink = (vmspageframe_t *)vmsCleanPageListHead.start;
            while(vms_EntryLink)
            {         
                if((vms_EntryLink->pnum == pnum) && (vms_EntryLink->vbit))
                {
                    efound = 1;
                    break;
                }
                vms_EntryLink = vms_EntryLink->next;
            }
            if(efound)
            {
                if(strcmp(mode,"debug")==0)
                   	printf("Page found in Clean list\n");
                if((vms_EntryLink->dbit == 'R') && (rw == 'W'))
                    vms_EntryLink->dbit = 'W';
                return;
            }
            else
            {
              //search in Dirty list
              i=0;
              vms_EntryLink = (vmspageframe_t *)vmsDirtyPageListHead.start;
                while(vms_EntryLink)
                {         
                    if((vms_EntryLink->pnum == pnum) && (vms_EntryLink->vbit))
                    {
                        efound = 1;
                        break;
                    }
                    vms_EntryLink = vms_EntryLink->next;
                }
                if(efound)
                {
                    if(strcmp(mode,"debug")==0)
                    	printf("Page found in Dirty list\n");
                    RemEleFromQueue((QHead_t *)&vmsDirtyPageListHead, (Queue_t *)vms_EntryLink);    
                    if((vms_EntryLink->dbit == 'R') && (rw == 'W'))
                        vms_EntryLink->dbit = 'W'; 
                    vms_TempEntryLink = (vmspageframe_t *)GetFromLast((QHead_t *)&vmsCleanPageListHead);
                    PutInStartQ((QHead_t *)&vmsDirtyPageListHead, (Queue_t *)vms_TempEntryLink);
                    PutInStartQ((QHead_t *)&vmsCleanPageListHead, (Queue_t *)vms_EntryLink);                     
                    return;
                }
                else
                {
                    vms_EntryLink = (vmspageframe_t *)GetFromLast((QHead_t *)&vmsDirtyPageListHead); //the victim
                    if(vms_EntryLink->dbit == 'W')
                       Writes++; //page is swapped to disk
                    Reads++;                    
                    //update the fields with new values and add to list
                    vms_EntryLink->pnum = pnum;
                    vms_EntryLink->dbit = 'R';
                    vms_TempEntryLink = (vmspageframe_t *)GetFromLast((QHead_t *)&vmsCleanPageListHead);
                    PutInStartQ((QHead_t *)&vmsCleanPageListHead, (Queue_t *)vms_EntryLink);
                    PutInStartQ((QHead_t *)&vmsDirtyPageListHead, (Queue_t *)vms_TempEntryLink);                        
                    if(strcmp(mode,"debug")==0)
                    	printf("Page swapped out\n");
                    return;
                }
            }
        }
    }
}	


//lru
void lru(unsigned int addr, char rw)
{
	int i,pnum,efound=0;
	struct lrupageframe *lru_TableEntry;	
	
	pnum = GetLog_PageNum(addr);
	//is page already present in pageFrame Memory??
	i=0;
	lru_TableEntry = lruTable;
	while(i<nFrames)
	{
		if((lru_TableEntry->pnum == pnum) && (lru_TableEntry->vbit))
		{
			efound = 1;
		}
		lru_TableEntry++;
		i++;
	}
	if(efound)
	{
        if(strcmp(mode,"debug")==0)
          	printf("Page already present in Page table\n");		
		UpdateRefVar(pnum, rw);
		return;
	}

	//find a free page at the start of programme. 
	efound = 0;
	i=0;
	lru_TableEntry = lruTable;
    while(i<nFrames)
	{
		if(!lru_TableEntry->vbit && !efound)
		{
			lru_TableEntry->vbit = 1;
			lru_TableEntry->occ = 0; //found, 
			lru_TableEntry->pnum = pnum;

			efound = 1;
		}
		lru_TableEntry++;
		i++;
	}

	if(efound)
	{                          //free page found.
 	    if(strcmp(mode,"debug")==0)
          	printf("Page loaded into page table\n");
		Reads++;
		UpdateRefVar(pnum, rw);
		return;
	}

    //Page is not free, page replacement algos are used.
	lru_TableEntry = LeastRecentlyUsedEntry();

	if(lru_TableEntry->dbit == 'W')
		Writes++;  						
	Reads++; //new page will be read from disk.
	lru_TableEntry->pnum = pnum;//updating the pageFrame entry
	lru_TableEntry->occ = 0;
	lru_TableEntry->dbit = 'R';    //intialize it to read type.
	if(strcmp(mode,"debug")==0)
    	printf("Page replaced using LRU\n");

}	




int main(int argc, char *argv[])
{
    unsigned int addr;
    char rw;
    FILE *f;
    char *tracename;
    tracename= argv[1];
    nFrames=atoi(argv[2]);
    char  *algo;
    algo= argv[3];
    mode= argv[4]; 
    //verification of command line arguments.
    if(argc != 5)
    {
        printf("Invalid number of arguments. Enter 5 arguments\n");
        return -1;
    }
    if(strcmp(argv[3],"vms")!= 0 && strcmp(argv[3],"lru")!=0)
    {
        printf("Invalid algorithm. Algo should be vms orlru\n");
        return -1;
    }

    if(strcmp(argv[4],"debug")!=0 && strcmp(argv[4],"quiet")!=0)
    {
        printf("Mode should be:debug|quiet\n");
        return -1;

    }
    if(strcmp(algo,"vms") == 0)
    {
        vmsTable = (vmspageframe_t *)calloc((size_t)(sizeof(struct vmspageframe)*nFrames), (size_t)1);//array space is allocated
	      InitTableLink();  //table is initialized.
        f = fopen(tracename, "r"); //trace file is read.
        while(fscanf(f,"%x %c",&addr,&rw) == 2)
        {
            Iterations++;
            vms(addr, rw);
        }

    }
    else
    {
        lruTable = (struct lrupageframe *)calloc((size_t)(sizeof(struct lrupageframe)*nFrames), (size_t)1);// array space is allocated
        InitT();  //table is initialized
        f = fopen(tracename, "r"); //trace file is read
        while(fscanf(f,"%x %c",&addr,&rw) == 2) 
        { 
	        Iterations++;
            lru(addr, rw); 
        } 

    }
    Output();
    free(lruTable);
    fclose(f);
}
