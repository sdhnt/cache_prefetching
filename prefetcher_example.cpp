#include "pin.H"

#include <stdlib.h>

#include "dcache_for_prefetcher.hpp"
#include "pin_profile.H"

// Simulation will be stop once this number of instructions is executed
#define STOP_INSTR_NUM 1000000000 //1b instrs
// Simulator heartbeat rate
#define SIMULATOR_HEARTBEAT_INSTR_NUM 100000000 //100m instrs

ofstream outFile;
Cache *cache;
UINT64 loads;
UINT64 stores;
UINT64 hits;
UINT64 cacheHits;
UINT64 prefHits;
UINT64 accesses, prefetches;
bool use_next_n_lines;
bool use_stride;
bool use_distance;
int sets;
int associativity;
int blockSize;
UINT64 iCount = 0;
int StrideT[64][4]={0};//Stride Table
int ntag=0;//tag count for stride prefetcher
int prevad=0;//previous address in distance prefetcher
int prevd=0;//previous distance in distance prefetcher
int ctr=0;//to store the tag count
int** DistanceT=new int*[64];
/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,    "pintool",
    "o", "data", "specify output file name");
KNOB<string> KnobPrefetcherType(KNOB_MODE_WRITEONCE, "pintool",
    "pref_type", "none", "specify type of prefetcher to be used");
KNOB<UINT32> KnobAggression(KNOB_MODE_WRITEONCE , "pintool",
    "aggr", "2", "the aggression of the prefetcher");
KNOB<UINT32> KnobCacheSets(KNOB_MODE_WRITEONCE, "pintool",
    "sets","64", "number of sets in the cache");
KNOB<UINT32> KnobLineSize(KNOB_MODE_WRITEONCE, "pintool",
    "b","4", "cache block size in bytes");
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool",
    "a","2", "cache associativity (1 for direct mapped)");

/* ===================================================================== */

// Print a message explaining all options if invalid options are given
INT32 Usage()
{
  cerr << "This tool represents a cache simulator." << endl;
  cerr << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

// Count the instructions
VOID docount()
{
  // Update instruction counter
  iCount++;
  // Print this message every SIMULATOR_HEARTBEAT_INSTR_NUM executed
  if (iCount % SIMULATOR_HEARTBEAT_INSTR_NUM == 0) {
    std::cerr << "Executed " << iCount << " instructions." << endl;
  }
  // Release control of application if STOP_INSTR_NUM instructions have been executed
  if (iCount == STOP_INSTR_NUM) {
    PIN_Detach();
  }
}

// Take checkpoints with stats in the output file
VOID TerminateSimulationHandler(VOID *v)
{
  outFile << "Accesses: " << accesses << " Loads: "<< loads << " Stores: " << stores <<   endl;
  outFile << "Hits: " << hits << endl;
  if (accesses > 0) outFile << "Hit rate: " << double(hits) / double(accesses) << endl;
  outFile << "Prefetches: " << prefetches << endl;
  outFile << "Prefetch hits: " << cache->getPrefHits() << endl;
  outFile << "Successful prefetches: " << cache->getSuccessfulPrefs() << endl;
  outFile.close();
  // Following outputs will be visible if the porgram detaches, but not if it terminates
  std::cout << endl << "PIN has been detached at iCount = " << iCount << endl;
  std::cout << endl << "Simulation has reached its target point. Terminate simulation." << endl;
  if (accesses > 0) std::cout << "Cache Hit Rate:\t" << double(hits) / double(accesses) << endl;
  std::exit(EXIT_SUCCESS);
}

/* ===================================================================== */

void prefetchNextNLines(UINT64 addr){
  for (int i = 1; i <= aggression; i++) {
    UINT64 nextAddr = addr + (i * blockSize) ;
    if (!cache->exists(nextAddr)) {  // Use the member function Cache::exists(UINT64) to query whehter a block exists in the cache w/o triggering any LRU changes (not after a demand access)
      cache->prefetchFillLine(nextAddr); // Use the member function Cache::prefetchFillLine(UINT64) when you fill the cache in the LRU way for prefetch accesses
      prefetches++;
    }
  }
}

///MY IMPLEMENTATION STARTS HERE
void stridePrefetcher(int addr, int pc){
  bool hit=false;
  for(int i=0;i<ntag;i++)
  {
    if(StrideT[i][0]==pc)//implies a hit
    {
      hit=true;
      int predad=StrideT[i][1]+StrideT[i][2];//the predicted address
      int prevad=StrideT[i][1];
      if(predad==addr)//if correct
      {//we now update the states
        //StrideT stores the state in column 4 for each entry represented as an integer from 1-4 for each state
        if (StrideT[i][3]==4)//colmun 4 stores state
        {StrideT[i][3]=3;}
        else
        {StrideT[i][3]=2;}
        StrideT[i][1]=addr;//column 1 stores address pc
        if(StrideT[i][3]==2)
        {
          for(int k=1;k<=aggression;k++)//to prefetch
          {
            UINT64 nextAddr = addr + (k * blockSize) ;
            if (!cache->exists(nextAddr)) {
            cache->prefetchFillLine(nextAddr);
            prefetches++;
          }//same as the previous N-lines fetch function
          }
          StrideT[i][1]=predad + (aggression* blockSize);
        }
      }//end if correct
      else //if not correct
      {
        if(StrideT[i][3]==1)
        StrideT[i][3]=3;//state changes as described in asssignent specifiacation
        else if(StrideT[i][3]==2)
        StrideT[i][3]=1;
        else if(StrideT[i][3]==3)
        StrideT[i][3]=4;
        if(StrideT[i][3]!=2)
        StrideT[i][2]=addr - prevad;//change to the Stride which is stored in column 2
        StrideT[i][1]=addr;//update prevad for entry on table
      }
    }//end hit
  }//end loop
  if(hit==false)//if no hits
  {
    int reqindex=0;
    if(ntag==64)//full
    reqindex=rand()%64;//randomly replaced
    else
    {reqindex=ntag; ntag++;}
    //Adding entry
    StrideT[reqindex][0]=pc; StrideT[reqindex][1]=addr;
    StrideT[reqindex][2]=0; StrideT[reqindex][3]=0;
  }//end no hits
}//end stridePrefetcher function


void distancePrefetcher(int addr){//the distance prefetcher

  int newd=addr-prevad;
  targeti=-1;

  for(int i=0;i<ctr;i++)
  {
    if(DistanceT[i][0]==newd)//hit
    {
      targeti=i;
      for(int k=1;k<=aggression;k++)//to prefetch
      {
        UINT64 predAddr = addr + DistanceT[i][k];
        if (!cache->exists(predAddr)) {
          cache->prefetchFillLine(predAddr);
          prefetches++;
        }
      }//end prefetch
    }//end hit
  }//end for loop

  if(targeti==-1)//not a hit
  {
    int reqindex=0;
    if(ctr==64)//full
    reqindex=rand()%64;//randomly replaced
    else
    {reqindex=ctr; ctr++;}int k=1;
    //to add
    DistanceT[reqindex][0]=newd;
    for(int k=1;k<=aggression;k++)
    {
      DistanceT[reqindex][k]=0;
    }
  }//end not hit

//we can now implement the training
for(int i=0;i<ctr;i++)
{
  if(DistanceT[i][0]==prevd)
  {
    bool hit=false;
    for(int k=1;k<=aggression;k++)//avaialablilty loop
    {
      if(DistanceT[i][k]==0)//is available
      {
        DistanceT[i][k]=newd;hit=true;
      }
    }//end avaialablilty loop
    if(hit==false)//nothing available
    {
      int a= rand() % aggression+1; DistanceT[i][a]=newd;//randomly overwrites some column
    }
  }//endif
}//end training loop

pred=newd;
prevad=addr;

}//end distancePrefetcher function
/* ===================================================================== */

void Load(ADDRINT addr, ADDRINT pc)
{
  accesses++;
  loads++;
  if (cache->probeTag(addr)) { // Use the function Cache::probeTag(UINT64) wehen you probe the cache after a demand access
    hits++;
  }
  else {
    cache->fillLine(addr); // Use the member function Cache::fillLine(addr) when you fill in the MRU way for demand accesses
    if (use_next_n_lines) prefetchNextNLines(addr); // Call the Next line prefetcher on a load miss
    else if(use_stride) stridePrefetcher(addr, pc);
    else if(use_distance) distancePrefetcher(addr);
  }

}

/* ===================================================================== */

//Action taken on a store
void Store(ADDRINT addr, ADDRINT pc)
{
  accesses++;
  stores++;
  if (cache->probeTag(addr)) hits++;
  else  cache->fillLine(addr);
}

/* ===================================================================== */

// Receives all instructions and takes action if the instruction is a load or a store
// DO NOT MODIFY THIS FUNCTION
void Instruction(INS ins, void * v)
{
  //Insert a call before every instruction to count them
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);

 // Insert a call for every Load and Store
  if (INS_IsMemoryRead(ins) && INS_IsStandardMemop(ins)) {
    INS_InsertPredicatedCall(
        ins, IPOINT_BEFORE, (AFUNPTR) Load,
        (IARG_MEMORYREAD_EA), IARG_INST_PTR, IARG_END);
 }
  if ( INS_IsMemoryWrite(ins) && INS_IsStandardMemop(ins))
  {
    INS_InsertPredicatedCall(
      ins, IPOINT_BEFORE,  (AFUNPTR) Store,
      (IARG_MEMORYWRITE_EA), IARG_INST_PTR, IARG_END);
  }
}

/* ===================================================================== */

// Gets called when the program finishes execution
void Fini(int code, VOID * v)
{
  TerminateSimulationHandler(v);
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    // Initialize stats
    hits = 0; accesses = 0; prefetches = 0;
    cacheHits = 0; prefHits = 0; loads = 0;
    stores = 0;
    use_next_n_lines = false;
    use_stride = false;
    use_distance = false;
    aggression = KnobAggression.Value();
    sets = KnobCacheSets.Value();
    associativity = KnobAssociativity.Value();
    blockSize =  KnobLineSize.Value();
    if (KnobPrefetcherType.Value() == "none") {
  	  std::cerr << "Not using any prefetcher" << std::endl;
    }
    else if (KnobPrefetcherType.Value() == "next_n_lines") {
  	  std::cerr << "Using Next-N-Lines Prefetcher."<< std::endl;
      use_next_n_lines = true;
    }
    else if (KnobPrefetcherType.Value() == "stride") {
  	  std::cerr << "Using Stride Prefetcher." << std::endl;
      use_stride = true;
    }
    else if (KnobPrefetcherType.Value() == "distance") {
  	  std::cerr << "Using Distance Prefetcher." << std::endl;
      use_distance = true;
      int i,j;
      for(i = 0; i < 64 ; i++){
        DistanceRPT[i] = new int[aggression + 1];
        for( j = 0; j < aggression + 1; j++){
          DistanceRPT[i][j] = 0;
        }
      }
    }
    else {
      std::cerr << "Error: No such type of prefetcher. Simulation will be terminated." << std::endl;
      std::exit(EXIT_FAILURE);
    }

   // create a data cache
    cache = new Cache(sets, associativity, blockSize);

    // create an output file with an appropriate name
    string outName(KnobOutputFile.Value());
    outFile.open(outName.c_str());

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_AddDetachFunction(TerminateSimulationHandler, 0);

    // Never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
