/**
 * @copyright (c) 2020 King Abdullah University of Science and Technology (KAUST).
 *                     All rights reserved.
 **/

/**
 *
 * @file Storage.cpp
 *
 * MLBS is a library provided by King Abdullah University of Science and Technology (KAUST)
 *
 * @version 1.0.0
 * @author Tariq Alturkestani
 * @date 2020-11-19
 *
 **/

#include "Storage.h"
namespace mlbs{	
Storage::Storage(){}	
Storage::~Storage(){}
void Storage::init(){
    get_affinity("main");
    if ((this->getRamSize()/_1gb()) > 120.0 ){  
        std::fprintf(stderr, "RAM snaps exceed 120 GB ram limit! MLBS cannot proceed.\n");
        exit(0);
    }
    int allocation = maxNumOfSnapsInRam;
    if ( allocation > numberOfIOSnaps) {
        allocation = numberOfIOSnaps;
    }
    for (int i = 0; i < allocation; i++){
        freeMem.push(i);
        snaps.push_back(getGrid( ));
    }
    if ( maxNumOfSnapsInRam >=numberOfIOSnaps) {
        maxNumOfSnapsInL2 = 0;
        maxNumOfSnapsInL3 = 0;
        canAppProceed = true;
        startReading = true;
        std::cerr << "no need for a helper! all is in memory!\n";
    }
    else{
        maxNumOfSnapsInL3 = numberOfIOSnaps - maxNumOfSnapsInRam - maxNumOfSnapsInL2 ;
        if (maxNumOfSnapsInL2 > 0)
        {
            helperThreads=  std::thread(&Storage::helperWithBB, this);
        }
        else
        {
            helperThreads=  std::thread(&Storage::helperOneLayerOneFile, this);
        }
    }
}
void Storage::finalize(){
    if( helperThreads.joinable()){
        helperThreads.join();
    }
}
void Storage::setData(int index, std::string snapName, float *dataToBeCopied, int64_t dataSize){
    #pragma omp parallel for 
    for (size_t i = 0 ; i < dataSize/ sizeof dataToBeCopied ; i++)
    {
        this->snaps[index][i] =  dataToBeCopied[i] ;
    }
}
float * Storage::getData(int index, std::string snapName, float * dataToGet, int64_t dataSize){
    std::swap(snaps[index],dataToGet);
    return dataToGet;
}
void Storage::write(std::string filename, float  * data , int64_t dataSize){
    int numTrials = 0;
    this->memIndex = -1;
    while (this->memIndex == -1){
        while ( ((this->memIndex = popFreeMemIndex()) == -1) && numTrials++ < 100)  {}
        if (this->memIndex == -1)
        {
            numTrials = 0;
        }
    }
    setData(this->memIndex, filename, data, dataSize);
    this->snapsMemLoc[filename] = memIndex;
    if (numberOfIOSnaps - currentSnapNumToWrite > maxNumOfSnapsInRam){
        this->pushWriteQueue(filename, memIndex );
        this->pushReadStack(filename );
    }
   
    this->currentSnapNumToWrite++;
}
float * Storage::getGrid()
{
    float * ret = (float*) malloc(snapSize); 
    if (ret == 0) {  
        fprintf(stderr, "malloc failed!" );
        exit(0);
    }
#pragma omp parallel for 
    for ( size_t i = 0 ; i < snapSize/(sizeof ret); i +=4096)
    {
        ret[i] = 0.0;
    }
	return ret;
	
}
void Storage::read(std::string filename, float * data , int64_t dataSize){
    int whereIsTheFile = -1;
    whereIsTheFile= this->snapsMemLoc[filename];
    if ( whereIsTheFile  == -1 )
    {
        this->memMiss++;
        while((whereIsTheFile= this->snapsMemLoc[filename]) == -1) {
            std::this_thread::yield();
        }
    }
    data = getData(whereIsTheFile, filename, data, dataSize);
    this->snapsMemLoc[filename] = -1;
    this->pushFreeMemIndex(whereIsTheFile);	
}
std::pair<std::string,int> Storage::popReadStack(){
    std::pair<std::string,int> ret = std::make_pair("",-1);
    if (!readStack.empty())
    {
        ret.first  = readStack.top();
        ret.second = readStack.size();
        readStack.pop();
    }
    return ret;
}
void Storage::pushReadStack(std::string filename){
    readStack.push(filename);
}
std::pair<std::string,int> Storage::popWriteQueue(){
    std::pair<std::string,int> ret = std::make_pair("null",-1);
    if (!writeQueue.empty())
    {
        ret = writeQueue.front();
        writeQueue.pop();
    }
    return ret;
}
void Storage::pushWriteQueue(std::string filename, int i){
    writeQueue.push(std::make_pair(filename,i));
}
int Storage::popFreeMemIndex(){
    int ret = -1;
    if (!freeMem.empty())
    {
        ret = freeMem.front();
        freeMem.pop();
    }
    return ret;
}
void Storage::pushFreeMemIndex(int i ){
    freeMem.push(i);
}
int Storage::getFreeMemSize(){
    return freeMem.size();
}
void Storage::switchToRead() {
    startReading=true;
    std::this_thread::yield();
}
void Storage::waitForHelperToSwitch() {
    while(canAppProceed.load()==false)
    { 
        std::this_thread::yield();
    }
}
#ifdef DATAWARP_ROOT_DIR

void Storage::helperWithBB(){
	int ret=0;
    int complete=0;
    int pending=0;
    int deferred=0;
    int failed=0;
    std::vector <std::string> stagedOutFiles;
    std::cerr << "Helper BB\n";
	this->stick_this_thread_to_core(helperCoreID);
    std::pair<std::string,int> pushTask;
    while (currentNumOfSnapsInDisks < maxNumOfSnapsInL2+maxNumOfSnapsInL3)
    {
        pushTask = popWriteQueue();
        if (pushTask.second != -1){
            snapsMemLoc[pushTask.first] = -1;
            double start=omp_get_wtime();
            l2AbsName= l2PathName + "/" + pushTask.first;
            l3AbsName= l3PathName + "/" + pushTask.first;
            snpFileOut.open(l2AbsName, std::ios::out | std::ios::binary);
            if (!snpFileOut.is_open()){
                std::cerr << "Could not open file [" <<  l2AbsName <<"]\n";
                exit(0);
            }
            snpFileOut.write((char*)&snaps[pushTask.second][0],snapSize);
            this->pushFreeMemIndex( pushTask.second);
            if(snpFileOut.bad())  {
                std::cerr<<"Writing to file failed"<<std::endl;
                exit(0);
            }
            snapFsLocation[pushTask.first] = 2; // snap is written to L2
            if ( ((maxNumOfSnapsInL2+maxNumOfSnapsInL3) - currentNumOfSnapsInDisks)  > maxNumOfSnapsInL2 )
            {
                snpFileOut.flush();
                ret = dw_stage_file_out(l2AbsName.c_str() , l3AbsName.c_str() , DW_STAGE_IMMEDIATE);
                stagedOutFiles.push_back(l2AbsName.c_str());
                stagedOutStack.push(pushTask.first);
                snapFsLocation[pushTask.first] = 3; // snap is pushed to L3
                if (ret != 0)
                {
                    std::cerr<<"Stage out filed for file: " << l2AbsName <<std::endl;
                    exit(0);
                }
            }
            this->currentNumOfSnapsInDisks++;
            snpFileOut.close();
        }
    }
    std::stringstream timeinfo;
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    timeinfo << std::put_time(&tm, "%d-%m-%Y--%H-%M-%S");
    std::fprintf(stderr, "clearing buffers[%s]\n", timeinfo.str().c_str());
    while (startReading.load()==false)
    {
        std::this_thread::yield();
    }
    std::fprintf(stderr, "starting to read [%s]\n", timeinfo.str().c_str());
    this->canAppProceed = true;
    std::pair<std::string,int> pullTaskPair ;
    std::string pullTask;
    int memSlot = -1;
    while (currentNumOfSnapsInDisks!=0)
    {
        pullTaskPair =popReadStack();
        if (pullTaskPair.second != -1)
        {
            pullTask =pullTaskPair.first;
            while ((memSlot = popFreeMemIndex()) == -1) {}
            double start=omp_get_wtime();
            if ( snapFsLocation.find(pullTask) != snapFsLocation.end() )
            {
                if (snapFsLocation[pullTask] == 2)
                {
                    l2AbsName= l2PathName + "/" + pullTask;
                    snpFileIn.open(l2AbsName, std::ios::in | std::ios::binary);
                    if (!snpFileIn.is_open())
                    {
                        std::cerr << "Could not open file [" <<  l2AbsName <<"]\n";
                        exit(0);
                    }
                    snpFileIn.read((char*)&snaps[memSlot][0],snapSize);
                    snpFileIn.close();
                    snapFsLocation[pullTask] = 1;
                }
                else if ( snapFsLocation[pullTask] == 3 )
                {
                    l2Miss++;
                    l2AbsName= l2PathName + "/" + pullTask;
                    l3AbsName= l3PathName + "/" + pullTask;
                    ret =  dw_stage_file_in(l2AbsName.c_str(), l3AbsName.c_str());
                    if (ret != 0)
                    {
                        std::cerr<<"Stage in filed for file: " << l3AbsName <<std::endl;
                        strerror(-ret);
                        exit(0);
                    }
                    ret = dw_wait_file_stage(l2AbsName.c_str());
                    snpFileIn.open(l2AbsName, std::ios::in | std::ios::binary);
                    if (!snpFileIn.is_open())
                    {
                        std::cerr << "Could not open file [" <<  l2AbsName <<"]\n";
                        exit(0);
                    }
                    snpFileIn.read((char*)&snaps[memSlot][0],snapSize);
                    snapFsLocation[pullTask] = 1;
                    snpFileIn.close();
                }
            }
            else{
                std::cerr << pullTask << " no record of this snap!! \n" ;
                exit(0);
            }
            
            this->snapsMemLoc[pullTask] = memSlot;
            this->currentNumOfSnapsInDisks--;
            while( true ) 
            {
                if ( !stagedOutStack.empty())
                {
                    if (snapFsLocation[stagedOutStack.top()] == 3){
                        l2AbsName= l2PathName + "/" + stagedOutStack.top();
                        l3AbsName= l3PathName + "/" + stagedOutStack.top();
                        ret =  dw_stage_file_in(l2AbsName.c_str(), l3AbsName.c_str());
                        if (ret != 0)
                        {
                            std::cerr<<"Stage in filed for file: " << l3AbsName <<std::endl;
                            strerror(-ret);
                            exit(0);
                        }
                        snapFsLocation[stagedOutStack.top()] = 2;
                        stagedOutStack.pop();
                        break;
                    }
                    else {
                        stagedOutStack.pop();
                    }
                }
                else{
                    break;
                }
            } 
        }
    }
}
#else
void Storage::helperWithBB()
{
    fprintf(stderr,"Datawarp is not loaded aborting\n");
    exit(0);
}

#endif

void Storage::helperOneLayerOneFile(){
	fprintf(stderr, "helperOneLayerOneFile\n");
	this->stick_this_thread_to_core(helperCoreID);
    get_affinity("helper", 1, helperCoreID);
    l3AbsName= l3PathName + "/" + snpFileName;
    snpFileOut.open(l3AbsName, std::ios::out | std::ios::app | std::ios::binary);
    if (!snpFileOut.is_open()){
        std::cerr << "Could not open file [" <<  l3AbsName <<"]\n";
        exit(0);
    }
    std::pair<std::string,int> pushTask;
    size_t dataWrote;
    while (currentNumOfSnapsInDisks < maxNumOfSnapsInL3){
        pushTask = popWriteQueue();
        if (pushTask.second != -1){
            snapsMemLoc[pushTask.first] = -1;
            snapsFileOffset[pushTask.first] = snpFileOut.tellp();;
            snpFileOut.write((char*)&snaps[pushTask.second][0],snapSize);
            this->pushFreeMemIndex( pushTask.second);
            if(snpFileOut.bad()) {
                std::cerr<<"Writing to file failed"<<std::endl;
                exit(0);
            }
            this->currentNumOfSnapsInDisks++;
        }
    }
    snpFileOut.close();
    snpFileIn.open(l3AbsName, std::ios::in | std::ios::binary);
    if (!snpFileIn.is_open()){
        std::cerr << "Could not open file [" <<  l3AbsName <<"]\n";
        exit(0);
    }
    std::stringstream timeinfo;
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    timeinfo << std::put_time(&tm, "%d-%m-%Y--%H-%M-%S");
    std::fprintf(stderr, "clearing buffers[%s]\n", timeinfo.str().c_str());
    while (startReading.load()==false){
        std::this_thread::yield();
    }
    std::fprintf(stderr, "starting to read [%s]\n", timeinfo.str().c_str());
    this->canAppProceed = true;
    std::pair<std::string,int> pullTaskPair ;
    std::string pullTask;
    int memSlot = -1;
    while (currentNumOfSnapsInDisks!=0){
        pullTaskPair =popReadStack();
        if (pullTaskPair.second != -1){
            pullTask =pullTaskPair.first;
            std::fprintf(stderr,"pulling ahead [%s]\n", pullTask.c_str());
            while ((memSlot = popFreeMemIndex()) == -1)
            {}
            snpFileIn.seekg(snapsFileOffset[pullTask],snpFileIn.beg);
            snpFileIn.read((char*)&snaps[memSlot][0],snapSize);
            
            this->snapsMemLoc[pullTask] = memSlot;
            this->currentNumOfSnapsInDisks--;
        }
    }
    snpFileIn.close();
    std::remove(l3AbsName.c_str());
}

int Storage::stick_this_thread_to_core(int coreID){
   cpu_set_t cpuset;
   CPU_ZERO(&cpuset);
   CPU_SET(coreID, &cpuset);
   pthread_t current_thread = pthread_self();
   return pthread_setaffinity_np(current_thread,
                             sizeof(cpu_set_t), &cpuset);
}
void Storage::get_affinity(std::string txt, int ID, int coreID){
    int s, j;
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0)
        handle_error_en(s, "pthread_getaffinity_np");
    fprintf(stderr,"Set returned by pthread_getaffinity_np() in [%s][%d][%d]:\n", txt.c_str(), ID,coreID);
    
}
}// namespace MLBS
