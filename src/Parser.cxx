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

/* Functions to read the content of output files and to return them as array
   Merged Output Files: array contains: begin-size-end-status
   Output Files: array contains: address-status */

#include "FOMTools/Parser.hpp"

int getLineNumber(std::ifstream &file){
    int counter = 0;
    std::string line;

    while(!file.eof()){
        std::getline(file, line);
        counter++;}
    counter --;
    return counter;
}

Array parseMergedOutputFiles(char* iterationFilename){
  
  std::ifstream file;
  std::string line;
  Array a;
  int iteration, size, status, status2, i = 0, n = 0;
  uint64_t adrHexBegin, adrBegin, adrEnd, adrHexEnd;

  file.open(iterationFilename);
  if(!file){
    std::cerr << "Could not open file " << iterationFilename << std::endl;
    a.size = 0;
    a.pointer = NULL;
    return a;
    }
  
  n = getLineNumber(file);
  file.clear();
  file.seekg(0, std::ios::beg);

  a.pointer = (uint64_t*) malloc(n * 4 * sizeof(uint64_t));
  a.size = n * 4 * sizeof(uint64_t);


  while (std::getline(file, line, '\n')){
    sscanf(line.c_str(),"%d\t0x%lx\t%ld\t0x%lx\t%ld\t%d\t%d\t%d\n",
      &iteration,&adrHexBegin,&adrBegin,&adrHexEnd,&adrEnd,&size,&status,&status2);
    a.pointer[i]=adrBegin;
    a.pointer[i+1]=size;
    a.pointer[i+2]=adrEnd;
    a.pointer[i+3]=status;
    i += 4;
  }
  file.close();
return a;
}

Array parseOutputFiles(char* iterationFilename,  int begin=-1, int end=-1){

  std::ifstream file;
  std::string line;
  Array a;
  int iteration, size, status, status2, i = 0, linenumber = 0, n = 0;
  uint64_t adrHex, adr;
  
  file.open(iterationFilename);
  if(!file){
    std::cerr << "Could not open file " << iterationFilename << std::endl; 
    a.size = 0;
    a.pointer = NULL;
    return a;
    }

  //Allocate the right array size
  std::getline(file, line, '\n');
  n = getLineNumber(file);
  file.clear();
  file.seekg(0, std::ios::beg);
  std::getline(file, line, '\n');
  if(begin != -1 && end != -1){
    if(begin < n && end <= n) 
      n = (end - begin);
    else if(begin < n && end > n)
      n = n - begin; 
    else{
      a.size = 0;
      a.pointer = NULL;
      return a;}    
   }
  a.pointer = (uint64_t*) malloc(n * 2 * sizeof(uint64_t));
  a.size = n * 2 * sizeof(uint64_t);
  if(begin == -1 || end == -1){
    while (std::getline(file, line, '\n')){
      sscanf(line.c_str(),"%d\t0x%lx\t%ld\t%d\t%d\n",
           &iteration,&adrHex,&adr,&status,&status2);
      a.pointer[i]=adr;
      a.pointer[i+1]=(uint64_t) status;
      i += 2;
    }
  }
  else{
    while (std::getline(file, line, '\n')){
      if (linenumber >= begin && linenumber < end){
        sscanf(line.c_str(),"%d\t0x%lx\t%ld\t%d\t%d\n",
           &iteration,&adrHex,&adr,&status,&status2);
        a.pointer[i]=adr;
        a.pointer[i+1]=(uint64_t) status;
        i += 2;
      }
     if (linenumber > end)
       break;
     linenumber += 1;
    }
  }
  file.close();
return a;
}
