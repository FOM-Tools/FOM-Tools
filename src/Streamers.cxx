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

#include "FOMTools/Streamers.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <cstring>
#include <ios>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <sys/time.h>

#define handle_error(msg)				\
  do { perror(msg); exit(EXIT_FAILURE); } while (0)
static const uintptr_t pageMask=(sysconf(_SC_PAGE_SIZE) - 1);


void timespec_add (struct timespec *left, struct timespec *right) {
    // long tmp1 = long(left->tv_sec + right->tv_sec);
    // long tmp2 = long(left->tv_nsec + right->tv_nsec);
  
    // if (tmp2 >= 1000000000)
    //  {  
    //     ++tmp1;
    //     tmp2 -= 1000000000;
    //  }
  left->tv_sec += right->tv_sec;
  left->tv_nsec+= right->tv_nsec;
  if (left->tv_nsec>1000000000l){
    left->tv_sec++;
    left->tv_nsec-= 1000000000l;
  }
}

static int timespec_sub (struct timespec *diff, long xsec, long xnsec, long ysec, long ynsec)
{
  diff->tv_sec=xsec-ysec;
  diff->tv_nsec=xnsec-ynsec;
  if(diff->tv_nsec<0){
    diff->tv_sec--;
    diff->tv_nsec+=1000000000;
  }
}


FOM_mallocHook::MemRecord::MemRecord(void* r){
  m_h.tsec=0;
  m_h.tnsec=0;
  m_h.timediffsec=0;
  m_h.timediffnsec=0;
  m_h.timediff2sec=0;
  m_h.timediff2nsec=0;
  m_h.allocType=0;
  m_h.addr=0;
  m_h.size=0;
  m_h.count=0;
  m_stacks=0;
  m_overlap=MemRecord::Undefined;
  if(r){
    auto hh=(FOM_mallocHook::header*)r;
    m_h=*hh;
    m_stacks=(FOM_mallocHook::index_t*)(hh+1);
  }
}

FOM_mallocHook::MemRecord::OVERLAP_TYPE FOM_mallocHook::MemRecord::getOverlap()const {return m_overlap;};

void FOM_mallocHook::MemRecord::setOverlap(FOM_mallocHook::MemRecord::OVERLAP_TYPE o){m_overlap=o;}

FOM_mallocHook::MemRecord::~MemRecord(){};

const FOM_mallocHook::header* const FOM_mallocHook::MemRecord::getHeader() const{
  return &m_h;
  }

const FOM_mallocHook::index_t* const FOM_mallocHook::MemRecord::getStacks(int *count) const {
  *count=m_h.count;
  return m_stacks;
}

uintptr_t FOM_mallocHook::MemRecord::getFirstPage()const {
  return (m_h.addr&(~pageMask));
}

uintptr_t FOM_mallocHook::MemRecord::getLastPage()const {
  return (m_h.addr+m_h.size)|pageMask;
}
long FOM_mallocHook::MemRecord::getTimeSec()const{
  return m_h.tsec;
}

long FOM_mallocHook::MemRecord::getTimeNSec() const{
  return m_h.tnsec;
}
long FOM_mallocHook::MemRecord::getTimeDiffSec()const{
  return m_h.timediffsec;
}

long FOM_mallocHook::MemRecord::getTimeDiffNSec() const{
  return m_h.timediffnsec;
}

char FOM_mallocHook::MemRecord::getAllocType() const{
  return m_h.allocType; 
}

uintptr_t FOM_mallocHook::MemRecord::getAddr() const{
  return m_h.addr;
}

size_t FOM_mallocHook::MemRecord::getSize() const{
  return m_h.size;

}

std::vector<FOM_mallocHook::index_t> FOM_mallocHook::MemRecord::getStacks() const{
  if((m_h.count==0)||(m_stacks==0)){
    return std::vector<FOM_mallocHook::index_t>();
  }
  return std::vector<FOM_mallocHook::index_t> (m_stacks,m_stacks+m_h.count);
}

/* READER CLASS
 */

FOM_mallocHook::Reader::Reader(std::string fileName):m_fileHandle(-1),
						     m_fileLength(0),m_fileName(fileName),m_fileBegin(0),m_fileStats(0),m_fileOpened(false)
   {
  if(m_fileName.empty())throw std::ios_base::failure("File name is empty");
  int inpFile=open(m_fileName.c_str(),O_RDONLY);
  if(inpFile==-1){
    std::cerr<<"Input file \""<<m_fileName<<"\" does not exist"<<std::endl;
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));
  }
  m_fileHandle=inpFile;
  struct stat sinp;
  if(fstat(m_fileHandle,&sinp)==-1){
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));    
  }
  if(sinp.st_size<sizeof(FOM_mallocHook::header)){
    throw std::length_error("Corrupt file. File is too short");
  }
  m_fileLength=sinp.st_size;
  m_fileOpened=true;
  //size_t nrecords=0;
  char buff[2050];
  m_fileStats=new FOM_mallocHook::FileStats();
  //std::cout << m_fileStats << " " << m_fileHandle << std::endl;
  m_fileStats->read(m_fileHandle,false);
  off_t hdrOff=::lseek(m_fileHandle,0,SEEK_CUR);
  ::lseek(m_fileHandle,0,SEEK_SET);
  m_fileBegin=mmap64(0,sinp.st_size,PROT_READ,MAP_PRIVATE,inpFile,0);
  if(m_fileBegin==MAP_FAILED){
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048))+"failed to mmap "+m_fileName);        
  }
  std::cout<<"Starting to scan the file. File should contain "<<
    m_fileStats->getNumRecords()<<" entries"<<std::endl;

  void* fileEnd=(char*)m_fileBegin+sinp.st_size;
  FOM_mallocHook::header *h=(FOM_mallocHook::header*)(((uintptr_t)m_fileBegin)+hdrOff);
  std::string outName=fileName+".txt";
int outFile=open(outName.c_str(),O_WRONLY|O_CREAT|O_TRUNC,(S_IRWXU^S_IXUSR)|(S_IRWXG^S_IXGRP)|S_IROTH);
  ssize_t buffPos=0;
  size_t maxBuf=16<<10;
  char buff2[16<<10];
  
  struct timespec overhead;
  long overhead_sec = 0.0; 
  long overhead_nsec = 0.0;
  while ((void*)h<fileEnd){
     
     MemRecord mr(h);
     const auto hdr=mr.getHeader();
     struct timespec diff;
     timespec_sub(&diff, hdr->timediff2sec, hdr->timediff2nsec, hdr->timediffsec, hdr->timediffnsec);    
     timespec_add(&overhead, &diff);
     buffPos+=snprintf(buff2+buffPos,maxBuf-buffPos,
                "%ld%09ld %d %lu %d %ld%09ld %ld%09ld \"",
                hdr->tsec,
                hdr->tnsec,//*msecRes),
                hdr->allocType,
                hdr->addr, hdr->size, hdr->timediffsec, hdr->timediffnsec, overhead_sec, overhead_nsec);
    overhead_sec = overhead.tv_sec;
    overhead_nsec = overhead.tv_nsec;
    int nStacks=0;
    auto stIds=mr.getStacks(&nStacks);
   // std::cout << nStacks << std::endl;
     for(int i=0;i< nStacks;i++){
        buffPos+=snprintf(buff2+buffPos,maxBuf-buffPos," %u",*(stIds+i));}
    buffPos+=snprintf(buff2+buffPos,maxBuf-buffPos,"\"\n");
    write(outFile,buff2,buffPos);
    h=(FOM_mallocHook::header*)(((FOM_mallocHook::index_t*)(h+1))+h->count);
    buffPos=0;
    }
    
 
    //catch(std::exception &ex){
    //std::cout<< h <<std::endl;}
    


  /*m_records.reserve(2357945446);//m_fileStats->getNumRecords());
  std::cout << "....." <<std::endl;
  void* fileEnd=(char*)m_fileBegin+sinp.st_size;
  FOM_mallocHook::header *h=(FOM_mallocHook::header*)(((uintptr_t)m_fileBegin)+hdrOff);
  while ((void*)h<fileEnd){
     try{ 
     MemRecord mr(h);
   /*  const auto hdr=mr.getHeader();
     std::cout<<"tsec= "<<hdr->tsec<<
       " tnsec= "<<hdr->tnsec<<
       " addr= "<<hdr->addr<<
       " size= "<<hdr->size<<
       " count= "<<hdr->count;*/
  //  std::cout << "test3" << std::endl;
   /* m_records.emplace_back((void*)h);
    h=(FOM_mallocHook::header*)(((FOM_mallocHook::index_t*)(h+1))+h->count);}
    catch(std::exception &ex){
    std::cout<< h <<std::endl;
    h=(FOM_mallocHook::header*)(((FOM_mallocHook::index_t*)(h+1))+h->count);
    std::cout << h << std::endl;
    
    m_records.emplace_back((void*)h);
    MemRecord mr(h);
    const auto hdr=mr.getHeader();
     std::cout<<"tsec= "<<hdr->tsec<<
       " tnsec= "<<hdr->tnsec<<
       " addr= "<<hdr->addr<<
       " size= "<<hdr->size<<
       " count= "<<hdr->count << " " << h->count;
    break;
     }
  }
  std::cout<<"Found "<<m_records.size()<<" records"<<std::endl;*/
}
const FOM_mallocHook::FileStats* FOM_mallocHook::Reader::getFileStats()const{
  return m_fileStats;
}

FOM_mallocHook::Reader::~Reader(){
  if(m_fileOpened){
    munmap(m_fileBegin,m_fileLength);
    close(m_fileHandle);
    m_records.clear();
  }
}

const FOM_mallocHook::MemRecord& FOM_mallocHook::Reader::at(size_t t){return m_records.at(t);}
size_t FOM_mallocHook::Reader::size(){return m_records.size();}

/* WRITER CLASS
 */

FOM_mallocHook::Writer::Writer(std::string fileName):m_fileName(fileName),m_fileHandle(-1),m_fileOpened(false),m_stats(0){
  if(m_fileName.empty())throw std::ios_base::failure("File name is empty");
  int outFile=open(m_fileName.c_str(),O_WRONLY|O_CREAT|O_TRUNC,(S_IRWXU^S_IXUSR)|(S_IRWXG^S_IXGRP)|(S_IROTH));
  if(outFile==-1){
    std::cerr<<"Can't open out file \""<<m_fileName<<"\""<<std::endl;
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));
  }
  m_fileHandle=outFile;
  m_stats=new FileStats();
  m_stats->setVersion(1);
  m_stats->setPid(getpid());
  m_stats->setStartTime(getProcessStartTime());
  size_t len=2048;
  char *buff=new char[len];
  if(!parseCmdline(buff,&len)){
    throw std::ios_base::failure(std::string("Parsing process commandline failed! ")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  
  m_stats->setCmdLine(buff,len);
  //for(auto &cl:m_stats->getCmdLine()){std::cerr<<cl<<" ";}std::cerr<<std::endl;
  m_stats->write(m_fileHandle,false);
  m_fileOpened=true;
  m_nRecords=0;
  m_maxDepth=0;
  delete[] buff;
}


bool FOM_mallocHook::Writer::closeFile(bool flush){
  if(m_fileOpened){
    if(flush){
      if(m_stats){
	//std::cerr<<"Nrecords= "<<m_nRecords<<" max depth="<<m_maxDepth<<std::endl;
	m_stats->setNumRecords(m_nRecords);
	m_stats->setStackDepthLimit(m_maxDepth);
	m_stats->write(m_fileHandle,false);
      }
      fsync(m_fileHandle);
    }
    close(m_fileHandle);
    delete m_stats;
    m_stats=0;
    m_fileOpened=false;
    return true;
  }else{
    return false;
  }
}

bool FOM_mallocHook::Writer::reopenFile(bool seekEnd){
  if(m_fileOpened){
    return false;
  }
  if(m_fileName.empty())throw std::ios_base::failure("File name is empty");
  int outFile=open(m_fileName.c_str(),O_WRONLY,(S_IRWXU^S_IXUSR)|(S_IRWXG^S_IXGRP)|(S_IROTH));
  if(outFile==-1){
    std::cerr<<"Can't open out file \""<<m_fileName<<"\""<<std::endl;
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));
  }
  m_fileHandle=outFile;
  m_fileOpened=true;
  if(seekEnd){
    ::lseek(outFile,0,SEEK_END);
  }
  return true;
}

FOM_mallocHook::Writer::~Writer(){
  if(m_fileOpened){
    if(m_stats){
      //std::cerr<<"Nrecords= "<<m_nRecords<<" max depth="<<m_maxDepth<<std::endl;
      m_stats->setNumRecords(m_nRecords);
      m_stats->setStackDepthLimit(m_maxDepth);
      m_stats->write(m_fileHandle,false);
    }
    fsync(m_fileHandle);
    close(m_fileHandle);
  }
  delete m_stats;
  m_stats=0;
}

bool FOM_mallocHook::Writer::parseCmdline(char* b,size_t *len){
  auto pid=getpid();
  char buff[201];
  snprintf(buff,200,"/proc/%d/cmdline",pid);
  int cmdHandle=::open(buff,O_RDONLY);
  ssize_t cmdLen=::read(cmdHandle,b,*len);
  if(cmdLen<0){
    close(cmdHandle);
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,200)));
  }else if(cmdLen==0){
    close(cmdHandle);
    return false;
  }else if(cmdLen==*len){//buffer may not be enough!
    std::cerr<<"Warning commandline may be incomplete! buffer filled up completely"<<std::endl;
    *len=cmdLen;
  }else{
    *len=cmdLen;
  }
  close(cmdHandle);
  return true;
}

time_t FOM_mallocHook::Writer::getProcessStartTime(){
  auto pid=getpid();
  char fbuff[64];
  snprintf(fbuff,63,"/proc/%d/stat",pid);
  char inpBuff[4096];
  for(int i=0;i<4096;i++)inpBuff[i]=0;
  unsigned long long mstart=0;
  int uptimeFD=::open("/proc/uptime",O_RDONLY);
  ssize_t uptimeSize=::read(uptimeFD,inpBuff,4096);
  if(uptimeSize>0){
    sscanf(inpBuff,"%llu",&mstart);
  }
  ::close(uptimeFD);
  int statFD=::open(fbuff,O_RDONLY);
  ssize_t statSize=::read(statFD,inpBuff,4096);
  unsigned long long jiffies=0;
  if(statSize>0){
    if(statSize==4096){
      std::cerr<<"Can't read stat file, it is too big"<<std::endl;
    }
    char* l=inpBuff+statSize;
    while(l>inpBuff){
      if(*l==')')break;
      l--;
    }
    l+=4;
    int count=0;
    while(count<18){
      if(*l==' ')count++;
      l++;
    }
    sscanf(l,"%llu",&jiffies);
  }
  ::close(statFD);
  return (time_t)(mstart+(jiffies/sysconf(_SC_CLK_TCK)));
}

bool FOM_mallocHook::Writer::writeRecord(const MemRecord&r){
  const auto  hdr=r.getHeader();
  int nStacks=0;
  auto stIds=r.getStacks(&nStacks);
  m_nRecords++;
  if(m_maxDepth<nStacks)m_maxDepth=nStacks;
  // std::cout<<"tsec= "<<hdr->tsec<<
  //   " tnsec= "<<hdr->tnsec<<
  //   " addr= "<<hdr->addr<<
  //   " size= "<<hdr->size<<
  //   " count= "<<hdr->count;
  // for(int i=0;i<nStacks;i++){
  //   std::cout<<" "<<stIds[i];
  // }
  // std::cout<<std::endl;
  if(write(m_fileHandle,hdr,sizeof(*hdr))!=sizeof(*hdr)){
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));
  }
  if(write(m_fileHandle,stIds,sizeof(*stIds)*nStacks)!=sizeof(*stIds)*nStacks){
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));
  }
  return true;
}

FOM_mallocHook::FileStats::FileStats(){
  m_hdr=new FileStats::fileHdr();
  strncpy(m_hdr->key,"FOM",4);
  m_hdr->ToolVersion=-1;
  m_hdr->numRecords=0;
  m_hdr->maxStacks=0;
  m_hdr->pid=0;
  m_hdr->cmdLength=0;
  m_hdr->cmdLine=0;
}

FOM_mallocHook::FileStats::~FileStats(){
  delete[] m_hdr->cmdLine;
  delete m_hdr;
}

int FOM_mallocHook::FileStats::getVersion()const{
  return m_hdr->ToolVersion;
}

size_t FOM_mallocHook::FileStats::getNumRecords()const{
  return m_hdr->numRecords;
}

size_t FOM_mallocHook::FileStats::getMaxStackLen()const{
  return m_hdr->maxStacks;
}

uint32_t FOM_mallocHook::FileStats::getPid()const{
  return m_hdr->pid;
}

std::vector<std::string> FOM_mallocHook::FileStats::getCmdLine()const{
  std::vector<std::string> cmds;
  if(m_hdr->cmdLength){
    char* c=m_hdr->cmdLine;
    char *cmdEnd=m_hdr->cmdLine+m_hdr->cmdLength;
    while(c<cmdEnd){
      std::string s(c);
      c+=s.length()+1;
      cmds.push_back(s);
    }
  }
  return cmds;
}

time_t FOM_mallocHook::FileStats::getStartTime()const{
  return m_hdr->startTime;
}

void FOM_mallocHook::FileStats::setVersion(int ver){
  m_hdr->ToolVersion=ver;
}

void FOM_mallocHook::FileStats::setNumRecords(size_t n){
  m_hdr->numRecords=n;
}

void FOM_mallocHook::FileStats::setStackDepthLimit(size_t l){
  m_hdr->maxStacks=l;
}

void FOM_mallocHook::FileStats::setPid(uint32_t p){
  m_hdr->pid=p;
}

void FOM_mallocHook::FileStats::setCmdLine(char* cmd,size_t len){
  delete m_hdr->cmdLine;
  m_hdr->cmdLength=len;
  if(len){
    m_hdr->cmdLine=new char[len+1];
    ::memcpy(m_hdr->cmdLine,cmd,len);
    m_hdr->cmdLine[len]='\0';
  }else{
    m_hdr->cmdLine=0; 
  }
}

void FOM_mallocHook::FileStats::setStartTime(time_t t){
  m_hdr->startTime=t;
}

int FOM_mallocHook::FileStats::read(int fd,bool keepOffset){
  if(fd<0){
    throw std::ios_base::failure("Invalid file descriptor in read()");
  }
  auto currPos=::lseek(fd,0,SEEK_CUR);
  if(currPos==-1){
    char buff[2048];
    throw std::ios_base::failure(std::string("Finding file offset failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  if(::lseek(fd,0,SEEK_SET)==-1){
    char buff[2048];
    throw std::ios_base::failure(std::string("File seek failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }

  if(::read(fd,m_hdr->key,sizeof(m_hdr->key))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Parsing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"Key="<<m_hdr->key<<sizeof(m_hdr->key)<<std::endl;
  if(::read(fd,&(m_hdr->ToolVersion),sizeof(m_hdr->ToolVersion))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Parsing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
    // std::cout<<"ToolVersion="<<m_hdr->ToolVersion<<" "<<sizeof(m_hdr->ToolVersion)<<std::endl;
  if(::read(fd,&(m_hdr->numRecords),sizeof(m_hdr->numRecords))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Parsing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"numRecords="<<m_hdr->numRecords<<" "<<sizeof(m_hdr->numRecords)<<std::endl;  
  if(::read(fd,&(m_hdr->maxStacks),sizeof(m_hdr->maxStacks))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Parsing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"maxStacks="<<m_hdr->maxStacks<<" "<<sizeof(m_hdr->maxStacks)<<std::endl;
  if(::read(fd,&(m_hdr->pid),sizeof(m_hdr->pid))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Parsing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"pid="<<m_hdr->pid<<" "<<sizeof(m_hdr->pid)<<std::endl;
  if(::read(fd,&(m_hdr->startTime),sizeof(m_hdr->startTime))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Parsing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"Start time="<<m_hdr->startTime<<" "<<sizeof(m_hdr->startTime)<<std::endl;
  if(::read(fd,&(m_hdr->cmdLength),sizeof(m_hdr->cmdLength))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Parsing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cerr<<"Cmdlength="<<m_hdr->cmdLength<<" "<<sizeof(m_hdr->cmdLength)<<std::endl;
  //std::cerr<<"cmdLength="<<m_hdr->cmdLength<<std::endl;
  delete[] m_hdr->cmdLine;
  m_hdr->cmdLine=0;
  if(m_hdr->cmdLength){
    m_hdr->cmdLine=new char[m_hdr->cmdLength+1];
    if(::read(fd,m_hdr->cmdLine,m_hdr->cmdLength)<0){
      char buff[2048];
      throw std::ios_base::failure(std::string("Parsing header failed")+
				   std::string(strerror_r(errno,buff,2048)));
    }
    // std::cout<<"cmdLine="<<m_hdr->cmdLine<<std::endl;
  }
  if(keepOffset){
    if(::lseek(fd,currPos,SEEK_SET)==-1){
      char buff[2048];
      throw std::ios_base::failure(std::string("Seek failed")+
				   std::string(strerror_r(errno,buff,2048)));
    }
    // std::cout<<"File Offset "<<currPos<<std::endl;
  }
  return 0;
}

int FOM_mallocHook::FileStats::write(int fd,bool keepOffset)const{

  if(fd<0){
    throw std::ios_base::failure("Invalid file descriptor in write()");
  }
  auto currPos=::lseek(fd,0,SEEK_CUR);
  if(currPos==-1){
    char buff[2048];
    throw std::ios_base::failure(std::string("Finding file offset failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"File Offset "<<currPos<<std::endl;
  if(::lseek(fd,0,SEEK_SET)==-1){
    char buff[2048];
    throw std::ios_base::failure(std::string("File seek failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  if(::write(fd,m_hdr->key,sizeof(m_hdr->key))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Writing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"Key="<<m_hdr->key<<sizeof(m_hdr->key)<<std::endl;
  if(::write(fd,&(m_hdr->ToolVersion),sizeof(m_hdr->ToolVersion))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Writing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"ToolVersion="<<m_hdr->ToolVersion<<" "<<sizeof(m_hdr->ToolVersion)<<std::endl;
  if(::write(fd,&(m_hdr->numRecords),sizeof(m_hdr->numRecords))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Writing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"numRecords="<<m_hdr->numRecords<<" "<<sizeof(m_hdr->numRecords)<<std::endl;  
  if(::write(fd,&(m_hdr->maxStacks),sizeof(m_hdr->maxStacks))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Writing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"maxStacks="<<m_hdr->maxStacks<<" "<<sizeof(m_hdr->maxStacks)<<std::endl;
  if(::write(fd,&(m_hdr->pid),sizeof(m_hdr->pid))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Writing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"pid="<<m_hdr->pid<<" "<<sizeof(m_hdr->pid)<<std::endl;
  if(::write(fd,&(m_hdr->startTime),sizeof(m_hdr->startTime))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Writing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cout<<"Start time="<<m_hdr->startTime<<" "<<sizeof(m_hdr->startTime)<<std::endl;
  if(::write(fd,&(m_hdr->cmdLength),sizeof(m_hdr->cmdLength))<0){
    char buff[2048];
    throw std::ios_base::failure(std::string("Writing header failed")+
				 std::string(strerror_r(errno,buff,2048)));
  }
  // std::cerr<<"Cmdlength="<<m_hdr->cmdLength<<" "<<sizeof(m_hdr->cmdLength)<<std::endl;
  if(m_hdr->cmdLength){
    if(::write(fd,m_hdr->cmdLine,m_hdr->cmdLength)<0){
      char buff[2048];
      throw std::ios_base::failure(std::string("Writing header failed")+
				   std::string(strerror_r(errno,buff,2048)));
    } 
    // std::cout<<"cmdLine="<<m_hdr->cmdLine<<std::endl;
  }
  if(keepOffset){
    if(::lseek(fd,currPos,SEEK_SET)==-1){
      char buff[2048];
      throw std::ios_base::failure(std::string("Seek failed")+
				   std::string(strerror_r(errno,buff,2048)));
    }
    // std::cout<<"File Offset "<<currPos<<std::endl;
  }
  return 0;
}

int FOM_mallocHook::FileStats::read(std::istream &in){
  return 0;
}

int FOM_mallocHook::FileStats::write(std::ostream &out){
  return 0;
}
/* 
INDEXING READER
*/

FOM_mallocHook::IndexingReader::IndexingReader(std::string fileName,uint indexPeriod):m_fileHandle(-1),
										      m_fileLength(0),m_fileName(fileName),
										      m_fileBegin(0),m_fileStats(0),m_fileOpened(false),
										      m_period(indexPeriod),m_remainder(0),
										      m_lastIndex(0),m_numRecords(0),
										      m_lastHdr(0)
{
  if(m_fileName.empty())throw std::ios_base::failure("File name is empty");
  int inpFile=open(m_fileName.c_str(),O_RDONLY);
  if(inpFile==-1){
    std::cerr<<"Input file \""<<m_fileName<<"\" does not exist"<<std::endl;
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));
  }
  m_fileHandle=inpFile;
  struct stat sinp;
  if(fstat(m_fileHandle,&sinp)==-1){
    char buff[2048];
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048)));    
  }
  if(sinp.st_size<sizeof(FOM_mallocHook::header)){
    throw std::length_error("Corrupt file. File is too short");
  }
  m_fileLength=sinp.st_size;
  m_fileOpened=true;
  //size_t nrecords=0;
  char buff[2050];
  m_fileStats=new FOM_mallocHook::FileStats();
  //std::cout << m_fileStats << " " << m_fileHandle << std::endl;
  m_fileStats->read(m_fileHandle,false);
  off_t hdrOff=::lseek(m_fileHandle,0,SEEK_CUR);
  ::lseek(m_fileHandle,0,SEEK_SET);
  m_fileBegin=mmap64(0,sinp.st_size,PROT_READ,MAP_PRIVATE,inpFile,0);
  if(m_fileBegin==MAP_FAILED){
    throw std::ios_base::failure(std::string(strerror_r(errno,buff,2048))+"failed to mmap "+m_fileName);        
  }
  
  std::cout<<"Starting to scan the file. File should contain "<<
    m_fileStats->getNumRecords()<<" entries"<<std::endl;
  void* fileEnd=(char*)m_fileBegin+sinp.st_size;
  FOM_mallocHook::header *h=(FOM_mallocHook::header*)(((uintptr_t)m_fileBegin)+hdrOff);
  m_records.reserve(m_fileStats->getNumRecords());
  if(m_period<1)m_period=100;
  size_t count=0;
  m_lastHdr=h;
  while ((void*)h<fileEnd){
    if((count%m_period)==0)m_records.emplace_back(h);
    h=(FOM_mallocHook::header*)(((FOM_mallocHook::index_t*)(h+1))+h->count);
    count++;
  }
  m_numRecords=count;
  m_remainder=((count-1)%m_period);
  std::cout<<"Counted "<<count<<" records. Created "<<m_records.size()<<" index points. Remaining "<< m_remainder<<" records"<<std::endl;
}

const FOM_mallocHook::FileStats* FOM_mallocHook::IndexingReader::getFileStats()const{
  return m_fileStats;
}

FOM_mallocHook::IndexingReader::~IndexingReader(){
  if(m_fileOpened){
    munmap(m_fileBegin,m_fileLength);
    close(m_fileHandle);
    m_records.clear();
  }
}

const FOM_mallocHook::RecordIndex FOM_mallocHook::IndexingReader::at(size_t t){
  if(t>=m_numRecords){
    char bu[500];
    snprintf(bu,500,"Asked for an index larger than number of records! t=%ld size=%ld",t,m_numRecords);
    throw std::length_error(bu);
  }
  size_t bucket=t/m_period;
  size_t offset=t-(bucket*m_period);
  if(offset==0){return m_records.at(bucket);}
  size_t d=t-m_lastIndex;
  if((d>0) &&(d<offset)){
    auto h=m_lastHdr;
    for(int i=0;i<d;i++){
      h=(FOM_mallocHook::header*)(((FOM_mallocHook::index_t*)(h+1))+h->count);
    }
    m_lastHdr=h;
  }else{
    auto h=m_records.at(bucket).getHeader();
    for(int i=0;i<offset;i++){
      h=(FOM_mallocHook::header*)(((FOM_mallocHook::index_t*)(h+1))+h->count);
    }
    m_lastHdr=h;
  }
  m_lastIndex=t;
  return FOM_mallocHook::RecordIndex(m_lastHdr);
}

size_t FOM_mallocHook::IndexingReader::size(){
  return (((m_records.size()-1)*m_period)+m_remainder+1);
}
size_t FOM_mallocHook::IndexingReader::indexedSize(){
  return m_records.size();
}

/*
// Record Index
*/
FOM_mallocHook::RecordIndex::RecordIndex(const FOM_mallocHook::header* h):m_h(0),m_overlap(MemRecord::Undefined){
  if(h){
    m_h=h;
  }
}
FOM_mallocHook::MemRecord::OVERLAP_TYPE FOM_mallocHook::RecordIndex::getOverlap()const {return m_overlap;};

void FOM_mallocHook::RecordIndex::setOverlap(FOM_mallocHook::MemRecord::OVERLAP_TYPE o){m_overlap=o;}

FOM_mallocHook::RecordIndex::~RecordIndex(){};

const FOM_mallocHook::header* const FOM_mallocHook::RecordIndex::getHeader() const{
  return m_h;
}

const FOM_mallocHook::index_t* const FOM_mallocHook::RecordIndex::getStacks(int *count) const {
  if(m_h){
    *count=m_h->count;
    return (FOM_mallocHook::index_t*)(m_h+1);
  }
  *count=0;
  return 0;

}

uintptr_t FOM_mallocHook::RecordIndex::getFirstPage()const {
  return (m_h->addr&(~pageMask));
}

uintptr_t FOM_mallocHook::RecordIndex::getLastPage()const {
  return (m_h->addr+m_h->size)|pageMask;
}
long FOM_mallocHook::RecordIndex::getTimeSec()const{
  return m_h->tsec;
}

long FOM_mallocHook::RecordIndex::getTimeNSec() const{
  return m_h->tnsec;
}

long FOM_mallocHook::RecordIndex::getT0Sec()const{
  return m_h->tsec;
}

long FOM_mallocHook::RecordIndex::getT0NSec() const{
  return m_h->tnsec;
}
long FOM_mallocHook::RecordIndex::getT1Sec()const{
  return m_h->timediffsec;
}

long FOM_mallocHook::RecordIndex::getT1NSec() const{
  return m_h->timediffnsec;
}
long FOM_mallocHook::RecordIndex::getT2Sec()const{
  return m_h->timediff2sec;
}

long FOM_mallocHook::RecordIndex::getT2NSec() const{
  return m_h->timediff2nsec;
}

char FOM_mallocHook::RecordIndex::getAllocType() const{
  return m_h->allocType; 
}

uintptr_t FOM_mallocHook::RecordIndex::getAddr() const{
  return m_h->addr;
}

size_t FOM_mallocHook::RecordIndex::getSize() const{
  return m_h->size;

}

std::vector<FOM_mallocHook::index_t> FOM_mallocHook::RecordIndex::getStacks() const{
  if((m_h==0)||(m_h->count==0)){
    return std::vector<FOM_mallocHook::index_t>();
  }
  return std::vector<FOM_mallocHook::index_t> ((FOM_mallocHook::index_t*)(m_h+1),((FOM_mallocHook::index_t*)(m_h+1))+m_h->count);
}

/*
FullRecord
*/

FOM_mallocHook::FullRecord::FullRecord(const FOM_mallocHook::header* h):m_h(0),m_overlap(MemRecord::Undefined){
  if(h){//make a local copy
    m_h=(FOM_mallocHook::header*)new char[sizeof(FOM_mallocHook::header)+(h->count*sizeof(FOM_mallocHook::index_t))];
    *m_h=*h;
    auto dst=((FOM_mallocHook::index_t*)(m_h+1));
    auto src=((FOM_mallocHook::index_t*)(h+1));
    for(int i=0;i<h->count;i++){
      src[i]=dst[i];
    }
  }
}

FOM_mallocHook::FullRecord::FullRecord(const FullRecord& rhs):m_h(0),m_overlap(rhs.m_overlap){
  if(rhs.m_h){//make a local copy
    m_h=(FOM_mallocHook::header*)new char[sizeof(FOM_mallocHook::header)+(rhs.m_h->count*sizeof(FOM_mallocHook::index_t))];
    *m_h=*rhs.m_h;
    auto dst=((FOM_mallocHook::index_t*)(m_h+1));
    auto src=((FOM_mallocHook::index_t*)(rhs.m_h+1));
    for(int i=0;i<rhs.m_h->count;i++){
      src[i]=dst[i];
    }
  }
}

FOM_mallocHook::MemRecord::OVERLAP_TYPE FOM_mallocHook::FullRecord::getOverlap()const {return m_overlap;};

void FOM_mallocHook::FullRecord::setOverlap(FOM_mallocHook::MemRecord::OVERLAP_TYPE o){m_overlap=o;}

FOM_mallocHook::FullRecord::~FullRecord(){delete[] (char*)m_h;};

const FOM_mallocHook::header* const FOM_mallocHook::FullRecord::getHeader() const{
  return m_h;
}

const FOM_mallocHook::index_t* const FOM_mallocHook::FullRecord::getStacks(int *count) const {
  if(m_h){
    *count=m_h->count;
    return (FOM_mallocHook::index_t*)(m_h+1);
  }
  *count=0;
  return 0;

}

uintptr_t FOM_mallocHook::FullRecord::getFirstPage()const {
  return (m_h->addr&(~pageMask));
}

uintptr_t FOM_mallocHook::FullRecord::getLastPage()const {
  return (m_h->addr+m_h->size)|pageMask;
}
long FOM_mallocHook::FullRecord::getTimeSec()const{
  return m_h->tsec;
}

long FOM_mallocHook::FullRecord::getTimeNSec() const{
  return m_h->tnsec;
}

long FOM_mallocHook::FullRecord::getT0Sec()const{
  return m_h->tsec;
}

long FOM_mallocHook::FullRecord::getT0NSec() const{
  return m_h->tnsec;
}
long FOM_mallocHook::FullRecord::getT1Sec()const{
  return m_h->timediffsec;
}

long FOM_mallocHook::FullRecord::getT1NSec() const{
  return m_h->timediffnsec;
}
long FOM_mallocHook::FullRecord::getT2Sec()const{
  return m_h->timediff2sec;
}

long FOM_mallocHook::FullRecord::getT2NSec() const{
  return m_h->timediff2nsec;
}

char FOM_mallocHook::FullRecord::getAllocType() const{
  return m_h->allocType; 
}

uintptr_t FOM_mallocHook::FullRecord::getAddr() const{
  return m_h->addr;
}

size_t FOM_mallocHook::FullRecord::getSize() const{
  return m_h->size;

}

std::vector<FOM_mallocHook::index_t> FOM_mallocHook::FullRecord::getStacks() const{
  if((m_h==0)||(m_h->count==0)){
    return std::vector<FOM_mallocHook::index_t>();
  }
  return std::vector<FOM_mallocHook::index_t> ((FOM_mallocHook::index_t*)(m_h+1),((FOM_mallocHook::index_t*)(m_h+1))+m_h->count);
}
