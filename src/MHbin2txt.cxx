/*
 *  Copyright (c) CERN 2015
 *
 *  Authors:
 *      Nathalie Rauschmayr <nathalie.rauschmayr_ at _ cern _dot_ ch>
 *      Sami Kama <sami.kama_ at _ cern _dot_ ch>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

// Output converter for malloc hook binary files to txt files
// mmap part copied from man pages

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <getopt.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <ctime>
#include <cstdint>
#include <iostream>
#include "FOMTools/Streamers.hpp"
#define handle_error(msg)                              \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)
static const off64_t pageMask=(sysconf(_SC_PAGE_SIZE) - 1);
static double msecRes=1.0/1000000;

void printUsage(char* name){
  std::cout<<"Usage:  "<<name<<" -i <input> -o <output> "<<std::endl;
  std::cout<<"     --input  (-i)  name of a file that is created by mallochook"<<std::endl;
  std::cout<<"     --output (-o)  output file name"<<std::endl;
}

int main(int argc,char* argv[]){
  std::string inpName("");
  std::string outName("");
  struct stat sinp;
  int c;
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"help", 0, 0, 'h'},
      {"input", 1, 0, 'i'},
      {"output", 1, 0, 'o'},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc, argv, "hi:o:",
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 'h':
      printUsage(argv[0]);
      exit(EXIT_SUCCESS);
      break;
    case 'i':  {
      inpName=std::string(optarg);
      break;
    }
    case 'o':  {
      outName=std::string(optarg);
      break;
    }
    default:
      printf("unknown parameter! getopt returned character code 0%o ??\n", c);
    }
  }
  if(optind<argc){
    while(optind<argc){
      if(inpName.empty()){
	inpName=argv[optind];
      }else if(outName.empty()){
	inpName=argv[optind];	
      }else{
	std::cerr<<"unknown parameter "<<argv[optind]<<std::endl;
      }
      optind++;
    }
  }

  if(inpName.empty()){
    std::cout<<"Input file name is needed"<<std::endl;
    printUsage(argv[0]);
    exit(EXIT_FAILURE);
  }

  if(outName.empty()){
    // std::cout<<"Output file name is needed"<<std::endl;
    // printUsage(argv[0]);
    // exit(EXIT_FAILURE);
    outName=inpName+".txt";
    std::cout<<"Using outputfile "<<outName<<std::endl;
  }

  int outFile=open(outName.c_str(),O_WRONLY|O_CREAT|O_TRUNC,(S_IRWXU^S_IXUSR)|(S_IRWXG^S_IXGRP)|S_IROTH);
  if(outFile==-1){
    std::cerr<<"Cant open output file \""<<outName<<std::endl;    
    handle_error("Opening output");
  }
  size_t nrecords=0;
  size_t maxBuf=16<<10;
  ssize_t buffPos=0;
  char buff[16<<10];
  ssize_t writtenBytes=0;
  size_t totBytes=0;
  struct timespec tstart,tend;
  struct stat st;
  if(lstat(inpName.c_str(),&st)){
    std::cerr<<"Can't stat input file \""<<inpName<<"\". Check that file exists and readeable"<<std::endl;
  }
  if(st.st_size> (7<<30)){//"file is too big, just use indexer to access it"
    std::cout<<"File is too big ("<<st.st_size<<" B) using indexer"<<std::endl;
    int rc=clock_gettime(CLOCK_MONOTONIC,&tstart);
    FOM_mallocHook::IndexingReader *r=0;
    try{
      r=new FOM_mallocHook::IndexingReader(inpName);
      int rc=clock_gettime(CLOCK_MONOTONIC,&tend);
      long ds=(tend.tv_sec-tstart.tv_sec);
      long dns=(tend.tv_nsec-tstart.tv_nsec);
      if(dns<0){
	ds--;
	dns+=1000000000;
      }
      dns=dns/1000000.;
      printf("Scanning file %s took %lu.%03lu seconds\n",inpName.c_str(),ds,dns);
    }catch(std::exception &ex){
      fprintf(stderr,"Caught exception %s\n",ex.what());
      exit(EXIT_FAILURE);
    }
    size_t nRecords=r->size();
    printf("Starting conversion of %ld records, (%ld x %d B)\n",nRecords,r->indexedSize(),sizeof(FOM_mallocHook::RecordIndex));
    for(size_t t=0;t<nRecords;t++){
      auto memRec=r->at(t);
      auto hdr=memRec.getHeader();
      buffPos+=snprintf(buff+buffPos,maxBuf-buffPos,
			"%ld.%ld %d 0x%lx %ld %ld.%ld",
			hdr->tsec,
			(long)(hdr->tnsec),//*msecRes),
			hdr->allocType,
			//((hdr->addr)&(~pageMask)),
			//((hdr->addr+hdr->size)|(pageMask)),
			hdr->addr, hdr->size, hdr->timediffsec, hdr->timediffnsec);
      int nStacks=0;
      auto stIds=memRec.getStacks(&nStacks);
      for(int i=0;i<nStacks;i++){
	buffPos+=snprintf(buff+buffPos,maxBuf-buffPos," %u",*(stIds+i));
      }
      buffPos+=snprintf(buff+buffPos,maxBuf-buffPos,"\n");
      writtenBytes=write(outFile,buff,buffPos);
      if(writtenBytes!=buffPos){
	if(writtenBytes==-1){
	  handle_error("write failed");
	}
	fprintf(stderr,"Buffer written partially\n");
	exit(EXIT_FAILURE);
      }
      totBytes+=writtenBytes;
      buffPos=0;
      nrecords++;
    }
    rc=clock_gettime(CLOCK_MONOTONIC,&tend);
    delete r;
    long ds=(tend.tv_sec-tstart.tv_sec);
    long dns=(tend.tv_nsec-tstart.tv_nsec);
    if(dns<0){
      ds--;
      dns+=1000000000;
    }
    dns=dns/1000000.;
    printf("Read %ld records and written %lu bytes to %s in %lu.%03lu seconds\n",nrecords,totBytes,outName.c_str(),ds,dns);
  }else{
    int rc=clock_gettime(CLOCK_MONOTONIC,&tstart);
    FOM_mallocHook::Reader *rdr=0;
    try{
      rdr=new FOM_mallocHook::Reader(inpName);
    }catch(std::exception &ex){
      fprintf(stderr,"Caught exception %s\n",ex.what());
      exit(EXIT_FAILURE);
    }
    size_t nRecords=rdr->size();
    printf("Starting conversion\n");
    for(size_t t=0;t<nRecords;t++){
      auto memRec=rdr->at(t);
      auto hdr=memRec.getHeader();
      buffPos+=snprintf(buff+buffPos,maxBuf-buffPos,
			"%ld.%ld %d 0x%lx %ld %ld.%ld",
			hdr->tsec,
			(long)(hdr->tnsec),//*msecRes),
			hdr->allocType,
			//((hdr->addr)&(~pageMask)),
			//((hdr->addr+hdr->size)|(pageMask)),
			hdr->addr, hdr->size, hdr->timediffsec, hdr->timediffnsec);
      int nStacks=0;
      auto stIds=memRec.getStacks(&nStacks);
    
      for(int i=0;i<nStacks;i++){
	buffPos+=snprintf(buff+buffPos,maxBuf-buffPos," %u",*(stIds+i));
      }
      buffPos+=snprintf(buff+buffPos,maxBuf-buffPos,"\n");
      writtenBytes=write(outFile,buff,buffPos);
      if(writtenBytes!=buffPos){
	if(writtenBytes==-1){
	  handle_error("write failed");
	}
	fprintf(stderr,"Buffer written partially\n");
	exit(EXIT_FAILURE);
      }
      totBytes+=writtenBytes;
      buffPos=0;
      nrecords++;
    }
    rc=clock_gettime(CLOCK_MONOTONIC,&tend);
    delete rdr;
    long ds=(tend.tv_sec-tstart.tv_sec);
    long dns=(tend.tv_nsec-tstart.tv_nsec);
    if(dns<0){
      ds--;
      dns+=1000000000;
    }
    dns=dns/1000000.;
    printf("Read %ld records and written %lu bytes to %s in %lu.%03lu seconds\n",nrecords,totBytes,outName.c_str(),ds,dns);
  }
  return 0;

}
