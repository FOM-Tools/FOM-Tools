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

#include "FOMTools/RegionFinder.hpp"

FOM_mallocHook::RegionFinder::RegionFinder(const std::string& fileName):m_rdr(0){
  m_rdr=new FOM_mallocHook::Reader(fileName);
}
FOM_mallocHook::RegionFinder::~RegionFinder(){
  delete m_rdr;
}

std::vector<FOM_mallocHook::MemRecord> FOM_mallocHook::RegionFinder::getAllocations(const RegionInfo &ri, FOM_mallocHook::RegionFinder::ALLOCTIME t)const{
  std::vector<FOM_mallocHook::MemRecord> regions;
  regions.reserve(100);
  size_t nRecords=m_rdr->size();
  if(t==FOM_mallocHook::RegionFinder::ANYTIME){
    for(size_t t=0;t<nRecords;t++){
      const auto &mr=m_rdr->at(t);
      auto ms=mr.getFirstPage();
      auto me=mr.getLastPage();
      if(ri.pBegin<me){ // rs<me
	if(ri.pBegin>=ms){//rs>=ms there is overlap
	  if(ri.pEnd<=me){//re<=me -> ms<=rs<re<=me ->superset Overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Superset);
	  }else{// re>me -> ms<=rs<me<re ->underflow
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Underflow); //isnt it Overflow?!
	  }
	}else{//rs<ms
	  if(ri.pEnd>=me){ //re>me ->rs<ms<me<re -> subset overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Subset);  
	  }else if(ri.pEnd>=ms){//re>ms -> rs<ms<re<me ->overflow overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Overflow); //underflow?!
	  }//re<ms -> rs<re<ms<me -> no overlap
	}
      }//rs>me -> ms<me<rs<re ->no overlap
      
    }
  } else if(t==FOM_mallocHook::RegionFinder::AFTER){
    for(size_t t=0;t<nRecords;t++){
      const auto &mr=m_rdr->at(t);
      if(mr.getTimeSec()<ri.t_sec || (mr.getTimeSec()==ri.t_sec && mr.getTimeNSec()<ri.t_nsec) ){
	continue;
      }
      auto ms=mr.getFirstPage();
      auto me=mr.getLastPage();
      if(ri.pBegin<me){ // rs<me
	if(ri.pBegin>=ms){//rs>=ms there is overlap
	  if(ri.pEnd<=me){//re<=me -> ms<=rs<re<=me ->superset Overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Superset);
	  }else{// re>me -> ms<=rs<me<re ->underflow
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Underflow); //isnt it Overflow?!
	  }
	}else{//rs<ms
	  if(ri.pEnd>=me){ //re>me ->rs<ms<me<re -> subset overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Subset);  
	  }else if(ri.pEnd>=ms){//re>ms -> rs<ms<re<me ->overflow overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Overflow); //underflow?!
	  }//re<ms -> rs<re<ms<me -> no overlap
	}
      }//rs>me -> ms<me<rs<re ->no overlap
      
    }
  }else{
    for(size_t t=0;t<nRecords;t++){
      const auto &mr=m_rdr->at(t);
      if(mr.getTimeSec()>ri.t_sec || (mr.getTimeSec()==ri.t_sec && mr.getTimeNSec()>ri.t_nsec) ){
	continue;
      }
      auto ms=mr.getFirstPage();
      auto me=mr.getLastPage();
      if(ri.pBegin<me){ // rs<me
	if(ri.pBegin>=ms){//rs>=ms there is overlap
	  if(ri.pEnd<=me){//re<=me -> ms<=rs<re<=me ->superset Overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Superset);
	  }else{// re>me -> ms<=rs<me<re ->underflow
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Underflow); //isnt it Overflow?!
	  }
	}else{//rs<ms
	  if(ri.pEnd>=me){ //re>me ->rs<ms<me<re -> subset overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Subset);  
	  }else if(ri.pEnd>=ms){//re>ms -> rs<ms<re<me ->overflow overlap
	    regions.push_back(mr);
	    regions.back().setOverlap(FOM_mallocHook::MemRecord::Overflow); //underflow?!
	  }//re<ms -> rs<re<ms<me -> no overlap
	}
      }//rs>me -> ms<me<rs<re ->no overlap
    }
  }
  return regions;
}

std::vector<std::vector<FOM_mallocHook::MemRecord> > FOM_mallocHook::RegionFinder::getAllocationSets(const std::vector<RegionInfo> &rVec, FOM_mallocHook::RegionFinder::ALLOCTIME t)const{
  if(rVec.size()==0)return std::vector<std::vector<FOM_mallocHook::MemRecord>>();
  std::vector<std::vector<FOM_mallocHook::MemRecord>> sregions(rVec.size());
  for(auto &i:sregions)i.reserve(100); 
  size_t nRecords=m_rdr->size();
  size_t nSets=rVec.size();
  if(t==FOM_mallocHook::RegionFinder::ANYTIME){
    for(size_t t=0;t<nRecords;t++){
      const auto &mr=m_rdr->at(t);
      auto ms=mr.getFirstPage();
      auto me=mr.getLastPage();
      for(size_t k=0;k<nSets;k++){
	const auto &ri=rVec[k];
	auto &regions=sregions[k];
	if(ri.pBegin<me){ // rs<me
	  if(ri.pBegin>=ms){//rs>=ms there is overlap
	    if(ri.pEnd<=me){//re<=me -> ms<=rs<re<=me ->superset Overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Superset);
	    }else{// re>me -> ms<=rs<me<re ->underflow
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Underflow); //isnt it Overflow?!
	    }
	  }else{//rs<ms
	    if(ri.pEnd>=me){ //re>me ->rs<ms<me<re -> subset overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Subset);  
	    }else if(ri.pEnd>=ms){//re>ms -> rs<ms<re<me ->overflow overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Overflow); //underflow?!
	    }//re<ms -> rs<re<ms<me -> no overlap
	  }
	}//rs>me -> ms<me<rs<re ->no overlap
      }
    }
  } else if(t==FOM_mallocHook::RegionFinder::AFTER){
    for(size_t t=0;t<nRecords;t++){
      const auto &mr=m_rdr->at(t);
      auto ms=mr.getFirstPage();
      auto me=mr.getLastPage();
      for(size_t k=0;k<nSets;k++){
	const auto &ri=rVec[k];
	auto &regions=sregions[k];
	
	if(mr.getTimeSec()<ri.t_sec || (mr.getTimeSec()==ri.t_sec && mr.getTimeNSec()<ri.t_nsec) ){
	  continue;
	}
	if(ri.pBegin<me){ // rs<me
	  if(ri.pBegin>=ms){//rs>=ms there is overlap
	    if(ri.pEnd<=me){//re<=me -> ms<=rs<re<=me ->superset Overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Superset);
	    }else{// re>me -> ms<=rs<me<re ->underflow
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Underflow); //isnt it Overflow?!
	    }
	  }else{//rs<ms
	    if(ri.pEnd>=me){ //re>me ->rs<ms<me<re -> subset overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Subset);  
	    }else if(ri.pEnd>=ms){//re>ms -> rs<ms<re<me ->overflow overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Overflow); //underflow?!
	    }//re<ms -> rs<re<ms<me -> no overlap
	  }
	}//rs>me -> ms<me<rs<re ->no overlap
      }
    }
  }else{
    for(size_t t=0;t<nRecords;t++){
      const auto &mr=m_rdr->at(t);
      auto ms=mr.getFirstPage();
      auto me=mr.getLastPage();
      for(size_t k=0;k<nSets;k++){
	const auto &ri=rVec[k];
	auto &regions=sregions[k];
	if(mr.getTimeSec()>ri.t_sec || (mr.getTimeSec()==ri.t_sec && mr.getTimeNSec()>ri.t_nsec) ){
	  continue;
	}
	if(ri.pBegin<me){ // rs<me
	  if(ri.pBegin>=ms){//rs>=ms there is overlap
	    if(ri.pEnd<=me){//re<=me -> ms<=rs<re<=me ->superset Overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Superset);
	    }else{// re>me -> ms<=rs<me<re ->underflow
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Underflow); //isnt it Overflow?!
	    }
	  }else{//rs<ms
	    if(ri.pEnd>=me){ //re>me ->rs<ms<me<re -> subset overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Subset);  
	    }else if(ri.pEnd>=ms){//re>ms -> rs<ms<re<me ->overflow overlap
	      regions.push_back(mr);
	      regions.back().setOverlap(FOM_mallocHook::MemRecord::Overflow); //underflow?!
	    }//re<ms -> rs<re<ms<me -> no overlap
	  }
	}//rs>me -> ms<me<rs<re ->no overlap
      }
    }
  }
  return sregions;   
}
