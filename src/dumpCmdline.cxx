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
}

int main(int argc,char* argv[]){
  std::string inpName("");
  //  std::string outName("");
  struct stat sinp;
  char* dataAddr=0;
  int c;
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"help", 0, 0, 'h'},
      {"input", 1, 0, 'i'},
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
    default:
      printf("unknown parameter! getopt returned character code 0%o ??\n", c);
    }
  }
  if(optind<argc){
    while(optind<argc){
      if(inpName.empty()){
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

  size_t nrecords=0;
  size_t maxBuf=16<<10;
  ssize_t buffPos=0;
  char buff[16<<10];
  ssize_t writtenBytes=0;
  size_t totBytes=0;
  struct timespec tstart,tend;
  int rc=clock_gettime(CLOCK_MONOTONIC,&tstart);
  FOM_mallocHook::ReaderBase *rdr=0;
  int inpFile=open(inpName.c_str(),O_RDONLY);
  auto fs=new FOM_mallocHook::FileStats();
  fs->read(inpFile,false);
  close(inpFile);
  fs->print(std::cout);
  // try{
  //   rdr=new FOM_mallocHook::Reader(inpName);
  // }catch(std::exception &ex){
  //   fprintf(stderr,"Caught exception %s\n",ex.what());
  //   exit(EXIT_FAILURE);
  // }
  // if(rdr){
  //   auto fs=rdr->getFileStats();
  //   fs->print();
  //   std::cout<<"Commandline was: "<<std::endl;
  //   int i=0;
  //   for(auto &c:fs->getCmdLine()){
  //     std::cout<<" argv["<<i<<"] = "<<c<<std::endl;
  //     i++;
  //   }
  //   std::cout<<"NumRecords= "<<fs->getNumRecords()<<std::endl;
  //   std::cout<<"Start time = "<<fs->getStartTime()<<std::endl;
  //   std::cout<<"Max stack depth= "<<fs->getMaxStackLen()<<std::endl;
  //   delete rdr;
  //   rdr=0;
  // }
  //munmap(dataAddr,sinp.st_size);
  return 0;
}
