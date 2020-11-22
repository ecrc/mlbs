/**
 * @copyright (c) 2020 King Abdullah University of Science and Technology (KAUST).
 *                     All rights reserved.
 **/

/**
 *
 * @file diver.cpp
 *
 * MLBS is a library provided by King Abdullah University of Science and Technology (KAUST)
 *
 * @version 1.0.0
 * @author Tariq Alturkestani
 * @date 2020-11-19
 *
 **/

#include <iostream>
#include <fcntl.h>
#include <string>
#include "Storage.h"
void * page_size_aligned_malloc(size_t size);
void work(mlbs::Storage & myStorage,int timeSteps,int snapRatio);
void fakecompute(int64_t numPoints);

int main (int argc, char ** argv){
    mlbs::Storage myStorage; 
    int nt = 250, snpRatio=25, nsnap=nt/snpRatio, snapsInRam=50;
    size_t gridSizeByte=512*1024*1024;
    std::string   snpFileName = "snap" ;
    std::string   L2Name = "./" ;
    int   snapsInL2 , snapsInL3 ;
    std::string   L3Name = "./" ;
    double start_MBS, elapse_meminit;
    gridSizeByte+= getpagesize() - ( gridSizeByte%getpagesize());
    start_MBS=omp_get_wtime();
    myStorage.setHelperCoreID(31);
    myStorage.setTimeSteps(nt);
    myStorage.setSnapRatio(snpRatio);
    myStorage.setNumOfIOSnaps(nsnap);
    myStorage.setSnapSize(gridSizeByte);
    myStorage.setMaxNumOfSnapsInRam(snapsInRam);
    myStorage.setL2NumOfSnaps(0);
    myStorage.setSnpFileName(snpFileName);
    myStorage.setL3PathName(L3Name);
    myStorage.init();
    elapse_meminit=omp_get_wtime()-start_MBS;
    std::cout << "init took " << elapse_meminit << "s" << std::endl;
    double startTime= omp_get_wtime();  
    work(myStorage, nt, snpRatio);
    double endTime =omp_get_wtime();    
    std::cout<< "Done in Time: " << endTime-startTime <<  std::endl;   
    myStorage.finalize();     
    return 0;
}
void fakecompute(int64_t numPoints){
    double numGigaPoints = numPoints / 1024.0/1024.0/1024.0;
    double speedGigaPointsPerSec = 3.5;
    double secondsToSleep = numGigaPoints/speedGigaPointsPerSec;
    int64_t ns = secondsToSleep*1e9;
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}
void work(mlbs::Storage & myStorage, int timeSteps, int snapRatio){
    int fd; 
    char label[128];
    int64_t dataSize = myStorage.getSnapSize();
    float * data;
    float * results;
    int64_t numberOfElms = dataSize/(sizeof*data);
    data    = (float *) page_size_aligned_malloc(dataSize);
    results = (float *) page_size_aligned_malloc(dataSize);
    #pragma omp parallel for  
    for (int64_t j= 0; j< numberOfElms; j++) {
        data[j]= 0.0;
    }
    #pragma omp parallel for  
    for (int64_t j= 0; j< numberOfElms; j++){
        results[j] = -1.0;
    }
    std::cout << "forward modeling stating.. " <<  std::endl;
    for (int i =1 ; i <= timeSteps; i++) {
        #pragma omp parallel for  
        for (int64_t j= 0; j< numberOfElms; j++){
            data[j]= (float)i;
        }
        fakecompute(numberOfElms);
        if (i% snapRatio == 0) {
            memset(label,'\0', 1);
            sprintf(label, "shot_%d",i);
            std::printf("writing %s ",label);
            double startTime= omp_get_wtime();
            myStorage.write(label, data, dataSize);
            double endTime =omp_get_wtime();
            std::printf("at speed, %.4f, gib/s\n",
                (dataSize/1024.0/1024.0/1024.0)/(endTime-startTime) );
        }
    }      
    std::printf( "backward stating..\n"); 
    myStorage.switchToRead();
    for (int i =timeSteps ; i > 0; i--)
    {
        #pragma omp parallel for  
        for (int64_t j= 0; j< numberOfElms; j++){
            results[j] += data[j];
        }
        if (i% snapRatio == 0) {
            sprintf(label, "shot_%d",i);
            std::printf("reading %s ",label);
            double startTime= omp_get_wtime();
            myStorage.read(label, data, dataSize);
            double endTime =omp_get_wtime();
            std::printf("at speed, %.4f, gib/s\n",
                (dataSize/1024.0/1024.0/1024.0)/(endTime-startTime) );
        }
        fakecompute(numberOfElms);
    }
    printf("(fake) writing final results [%s]\n", label);
    fd = open("results", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    ::write(fd,results,dataSize);
    close(fd); free(data); free(results);
}  
void * page_size_aligned_malloc(size_t size){
    size_t pageSize;
    void *aligned;
    pageSize = getpagesize();
    posix_memalign(&aligned, pageSize, size);
    return aligned;
}
