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


#include "FOMTools/MergePages.hpp"
#include <cstdint>
#include <cstdio>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <iostream>

bool MergePages(const std::string in,const std::string out,
		const std::function<bool(const RegionInfo&,const RegionInfo&)> *comp){

  if(in.empty()){
    std::cerr<<"Input file name can't be empty"<<std::endl;
    return false;
  }

  FILE *inFile=fopen(in.c_str(),"r");
  if(inFile==NULL){
    std::cerr<<"Failed to open input file "<<in<<std::endl;
    return false;
  }

  std::string outName=in+".merged";
  if(comp)outName+=".sorted";
  if(out.empty()){
    outName=out;
  }

  FILE *outFile=fopen(outName.c_str(),"w");
  size_t buffLen=400;
  ssize_t read=0;
  char *buff=(char*)malloc(sizeof(char)*buffLen);
  unsigned long adrx,adr,addrLast,rBegin=0;
  int iter,b1,b2,b3,b4,bitLast=0,bitCurr=0,count=0,b12,b34;

  if(!comp){
    while((read=getline(&buff,&buffLen,inFile))!=-1){
      int nelem=sscanf(buff,"%d\t0x%lx\t%lu\t%1d%1d\t%1d%1d",  &iter,&adrx,&adr,&b1,&b2,&b3,&b4);
      if(nelem==7){
	bitCurr=((((((b1<<1)|b2)<<1)|b3)<<1)|b4);
	if(bitCurr!=bitLast){
	  if(rBegin!=0)fprintf(outFile,"%d\t0x%lx\t%lu\t0x%lx\t%lu\t%d\t%d\n", iter,rBegin,rBegin,addrLast,addrLast,count,bitLast);
	
	  rBegin=adr;
	  b12=10*b1+b2;
	  b34=10*b3+b4;
	  count=0;
	  bitLast=bitCurr;
	}
	count++;
	addrLast=adr;
      }
    }
    fprintf(outFile,"%d\t0x%lx\t%lu\t0x%lx\t%lu\t%d\t%d\n", iter,rBegin,rBegin,addrLast,addrLast,count,bitLast);
  }else{
    std::vector<RegionInfo> regions;
    RegionInfo r;

    while((read=getline(&buff,&buffLen,inFile))!=-1){
     int nelem=sscanf(buff,"%d\t0x%lx\t%lu\t%1d%1d\t%1d%1d", &iter,&adrx,&adr,&b1,&b2,&b3,&b4);
     if(nelem==7){
	//bitCurr=((((((b1<<1)|b2)<<1)|b3)<<1)|b4);
	//bitCurr=b1*1000+b2*100+b3*10+b4;
	bitCurr = b1*10+b2;
	if(bitCurr!=bitLast){
	  r.pBegin=rBegin;
	  r.pEnd=addrLast;
	  r.size=count;
	  r.flags=bitLast;
	  regions.push_back(r);
	  count=0;
	  bitLast=bitCurr;
	  rBegin=adr;
	}
	count++;
	addrLast=adr;
      }
    }
    r.pBegin=rBegin;
    r.pEnd=addrLast;
    r.size=count;
    r.flags=bitLast;
    regions.push_back(r);
    count=0;
    bitLast=bitCurr;
    rBegin=adr;
 
    std::sort(regions.begin(),regions.end(),*comp);
    for(auto& i : regions){
      if(i.pBegin > 0)
      fprintf(outFile,"%d\t0x%lx\t%ld\t0x%lx\t%ld\t%lu\t%d\n", iter,i.pBegin,i.pBegin,i.pEnd,i.pEnd,i.size,i.flags);
    }
  }
  fclose(inFile);
  fclose(outFile);
  return true;
}
