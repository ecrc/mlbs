/**
 * @copyright (c) 2020 King Abdullah University of Science and Technology (KAUST).
 *                     All rights reserved.
 **/

/**
 *
 * @file Storage.h
 *
 * MLBS is a library provided by King Abdullah University of Science and Technology (KAUST)
 *
 * @version 1.0.0
 * @author Tariq Alturkestani
 * @date 2020-11-19
 *
 **/

#pragma once
#include <errno.h>
#include <iostream>
#include <string>
#include <string.h>
#include <cstdint>
#include <thread>
#include <queue>
#include <algorithm>
#include <limits>
#include <random>
#include <utility>
#include <vector>
#include <map>
#include <stack>
#include <cassert>
#include <unistd.h>
#include <omp.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <sys/time.h> 
#include <atomic>
#ifdef DATAWARP_ROOT_DIR
extern "C" {
#include <datawarp.h>
}
#endif
#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)
namespace mlbs
{
class Storage
{
public:
	Storage();
	~Storage();
	void init();
	void finalize();
	void setHelperCoreID(int id)  {helperCoreID=id;}
	void setTimeSteps(int ts ) {timeSteps=ts;}
	void setSnapRatio( int sr ) {snapRatio=sr;}
	void setNumOfIOSnaps( int iosnaps) {numberOfIOSnaps=iosnaps;}
    void setSnapSize( size_t sz) {snapSize=sz;}
    void setMaxNumOfSnapsInRam(int sc ) {maxNumOfSnapsInRam=sc;}
    void setL2NumOfSnaps(int sc ) {maxNumOfSnapsInL2=sc;}
    void setL3NumOfSnaps(int sc ) {maxNumOfSnapsInL3=sc;}
    void setL2PathName(std::string pn ) {l2PathName=pn;}
    void setL3PathName(std::string pn ) {l3PathName=pn;}
    void setSnpFileName(std::string pn ) {snpFileName=pn;}
	int getHelperCoreID() {return helperCoreID; }
	int getTimeSteps() {return timeSteps; }
	int getSnapRatio() {return snapRatio; }
	int getNumOfIOSnaps() {return numberOfIOSnaps;}
    size_t getSnapSize() {return snapSize;}
    int getMaxNumOfSnapsInRam() {return maxNumOfSnapsInRam;}
    int getMaxNumOfSnapsInL2() {return maxNumOfSnapsInL2;}
    int getMaxNumOfSnapsInL3() {return maxNumOfSnapsInL3;}
    int getL2Miss() {return l2Miss;}
	int64_t getRamSize(){return maxNumOfSnapsInRam*snapSize;}
	int getCurrentNumOfSnapsInRam(){return currentNumOfSnapsInRam;}
	int getCurrentNumOfSnapsInDisk(){return currentNumOfSnapsInDisks;}
	int getMemMisses(){return memMiss;}
    void setData(int index, std::string snapName, float *dataToBeCopied, int64_t dataSize);
    float * getData(int index, std::string snapName, float * dataToGet, int64_t dataSize);
    void write(std::string snapname, float * data , int64_t dataSize);
	void read(std::string snapname, float * data , int64_t dataSize);
    void switchToRead();
    void waitForHelperToSwitch();
private:
	int helperCoreID;
	int timeSteps;
    int snapRatio;
	int numberOfIOSnaps;
	size_t snapSize;
	int maxNumOfSnapsInRam;
	int maxNumOfSnapsInL2;
	int maxNumOfSnapsInL3;
    std::string l2PathName;
    std::string l3PathName;
    std::string l2AbsName;
    std::string l3AbsName;
    std::string snpFileName;
	int memIndex;
    int currentNumOfSnapsInRam;
    int currentNumOfSnapsInL2;
    int currentNumOfSnapsInL3;
	int  currentNumOfSnapsInDisks;
    std::atomic<bool> canAppProceed;
    std::atomic<bool> startReading;
    int memMiss;
    int l2Miss;
    int currentSnapNumToWrite; 
    std::thread helperThreads;    
    std::queue<int> freeMem;
	std::vector<float *> snaps;
    std::map<std::string, int> snapsMemLoc;
    std::map<std::string, size_t> snapsFileOffset;
    std::stack<std::string> readStack;
    std::queue<std::pair<std::string, int>> writeQueue;
    std::map<std::string, int> snapFsLocation;
    std::stack<std::string> stagedOutStack;
    std::string lastSnapRead;
    std::ofstream snpFileOut;
    std::ifstream snpFileIn;
    int popFreeMemIndex();
    void pushFreeMemIndex(int i);
    int getFreeMemSize();
    std::pair<std::string,int>  popReadStack();
    void pushReadStack(std::string filename);
    std::pair<std::string,int> popWriteQueue();
    void pushWriteQueue(std::string filename, int i);
	void helperWithBB();
	void helperOneLayerOneFile();
    float * getGrid();
	int stick_this_thread_to_core(int coreID);
	void get_affinity(std::string txt, int ID=0, int coreID=0);
    inline double _1gb() {return 1073741824.0;}
};
}
