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

// simple malloc interposer with stack trace using libunwind.

#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <dlfcn.h>
#include <cxxabi.h>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <map>
#include <vector>
#include <ctime>
#include <cstdint>
#include <limits>
#include <pthread.h>
#ifndef __DO_GNU_BACKTRACE__ 
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#else
#include <execinfo.h>
#include <errno.h>
#endif
#include "FOMTools/Streamers.hpp"

static std::atomic_flag malloc_tracing_flag_sami = ATOMIC_FLAG_INIT;
static std::atomic_flag calloc_tracing_flag_sami = ATOMIC_FLAG_INIT;
static std::atomic_flag initializedForkHooks = ATOMIC_FLAG_INIT;

#define __MAXBUFF__ 4096
static int maxBuff=__MAXBUFF__;

char* fileBuff(){
  static char fileBuff[__MAXBUFF__];
  return fileBuff;
}
//static int buffPos=0;
//static FILE* s_OutFil=0;
static FOM_mallocHook::Writer* fwriter=0;
static int dlsymBuffPos=0;
//static double msecRes=1.0/1000000;
extern "C" {
  void* malloc(size_t size);
  void* realloc(void* ptr,size_t size) throw();
  void* calloc(size_t n,size_t s);
}

char* dlsymBuff(){
  static char dlsymBuff[4096];
  return dlsymBuff;
}

std::map<unw_word_t,FOM_mallocHook::index_t>& symMap(){
  static  auto symMap=new std::map<unw_word_t,FOM_mallocHook::index_t>();
  return *symMap;
}

std::vector<std::string>& symNames(){
  static auto symNames=new std::vector<std::string>();
  return *symNames;
}
const char* getOutputFileName();


FOM_mallocHook::Writer* getWriter();
namespace FOM_mallocHook{

  class MallocBuildInfo{
  public:
    MallocBuildInfo(const std::string& s=""){};//std::cout<<"Malloc Hook built on "<<__DATE__<<" "<<__TIME__<<" @ "<<s<<std::endl;}
  };
}

//static FOM_mallocHook::MallocBuildInfo *mhbuildInfo=new FOM_mallocHook::MallocBuildInfo("");
static FOM_mallocHook::MallocBuildInfo *mhbuildInfo=0;

//debug with set exec-wrapper env 'LD_PRELOAD=...'
void atexit_handler(){
  if(fwriter==0){
    return;
  }
  malloc_tracing_flag_sami.test_and_set();
  calloc_tracing_flag_sami.test_and_set();
  char buff[2048];
  const char* fileN=getOutputFileName();
  snprintf(buff,2048,"%s_maps",fileN);
  errno=0;
  FILE* tmp=fopen(buff,"w+");
  if(tmp!=NULL){
    FILE* maps=fopen("/proc/self/maps","r");
    if(maps!=NULL){
      while(!feof(maps)){
	size_t nelem=fread(buff,sizeof(char),2048,maps);
	fwrite(buff,sizeof(char),nelem,tmp);
      }
      fclose(maps);
    }
    fflush(tmp);
    fclose(tmp);
  }
  snprintf(buff,2048,"%s_symbolLookupTable",fileN);
  tmp=fopen(buff,"w+");
  if(tmp!=NULL){
    FILE* cmdline=fopen("/proc/self/cmdline","r");
    if(cmdline!=NULL){//write commandline to output
      while(!feof(cmdline)){
	size_t nelem=fread(buff,sizeof(char),2048,cmdline);
	fwrite(buff,sizeof(char),nelem,tmp);
      }
      fwrite("\n",sizeof(char),strlen("\n"),tmp);
      fclose(cmdline);
    }
    for(size_t i=0;i<symNames().size();i++){
      fprintf(tmp,"%ld\t%s\n",i,symNames().at(i).c_str());
    }
    fflush(tmp);
    fclose(tmp);
  }
  delete fwriter;
  fwriter=0;
  delete mhbuildInfo;
  mhbuildInfo=0;
  delete &symNames();
  delete &symMap();
}

void prepFork(){
  if(fwriter){
    if(!malloc_tracing_flag_sami.test_and_set()){
      fwriter->closeFile(false);
      malloc_tracing_flag_sami.clear();
    }
  }
  
}

void postForkParent(){
  if(fwriter){
    fwriter->reopenFile(true);
  }
}

void postForkChildren(){
  if(fwriter){
    if(!malloc_tracing_flag_sami.test_and_set()){
      delete fwriter;
      fwriter=getWriter();
      malloc_tracing_flag_sami.clear();
    }
  }
  //std::cout<<"Called postForkChildren @ pid="<<getpid()<<std::endl;
  std::atexit(atexit_handler);
}

void show_backtrace (FILE* f,size_t size,void* addr,int depth,int allocType) {
  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip;
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  int count=0;
  struct timespec tp;
  int rc=clock_gettime(CLOCK_MONOTONIC_COARSE,&tp);
  FOM_mallocHook::header *hdr=(FOM_mallocHook::header*)(fileBuff());
  hdr->tsec=(long)tp.tv_sec;                  //time sec
  hdr->tnsec=(long)tp.tv_nsec;                 //time 
  hdr->allocType=(char)allocType;
  hdr->addr=(uintptr_t)addr;                       //returned addres
  hdr->size=size;                       //size of allocation
  FOM_mallocHook::index_t *stackRecord=(FOM_mallocHook::index_t*)(hdr+1);
  while (unw_step(&cursor) > 0 && count<depth) {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    auto it=symMap().insert(std::make_pair(ip,symMap().size()));
    //fprintf(stderr," ip=%ld\n",(long)ip);
    if(it.second){// new IP
      if(symMap().size()>=std::numeric_limits<FOM_mallocHook::index_t>::max()){
	std::cerr<<" Malloc hook has reached its indexing capacity. Please recompile with a wider index_t. Aborting!"<<std::endl;
	std::abort();
      }
      unw_word_t  offp=0,sp=0;
      size_t bufflen=1024;
      char funcName[bufflen];
      funcName[0]='\0';
      char strBuf[1400];

      int rc=unw_get_reg(&cursor, UNW_REG_SP, &sp);
      if(rc!=0){
	snprintf(strBuf,1400,"FAILED  @ ip= 0 sp= 0 RC=%d",rc);
      }else{
	rc=unw_get_proc_name (&cursor, funcName, bufflen, &offp);
	if(rc!=0){
	  snprintf(strBuf,1400,"FAILED @ ip= 0x%lx sp= 0x%lx %d",(long)ip,(long)sp,rc);
	}else{
	  snprintf(strBuf,1400,"%s + 0x%lx @ ip= 0x%lx sp= 0x%lx",funcName,(long)offp,(long)ip,(long)sp);
	}
      }
      symNames().emplace_back(strBuf);
    }
    *stackRecord=it.first->second;
    stackRecord++;
    count++;
  }
  hdr->count=count;
  fwriter->writeRecord(FOM_mallocHook::MemRecord(fileBuff()));
}

#ifdef __DO_GNU_BACKTRACE__
void show_backtrace_GNU (FILE* f,size_t size,void* addr,int depth) {
  void *bt[1024];
  int bt_size;
  char **bt_syms;
  int i;
  bt_size = backtrace(bt, 1024);
  bt_syms = backtrace_symbols(bt, bt_size);
  for (i = 1; i < bt_size; i++) {
    size_t len = strlen(bt_syms[i]);
    buffPos+=snprintf(fileBuff()+buffPos,maxBuff-buffPos,  "[%02d]  %s pages= 0x%x - 0x%x size=%ld, addr=0x%lx\n", i, bt_syms[i], ((long)addr)&(~0x0FFFl),(((long)addr)+size)|(0x0FFFl), size,addr);
    if(buffPos>(maxBuff-1500)){
      fwrite(fileBuff(),sizeof(char),buffPos,f);
      buffPos=0;}
    
    //full_write(STDERR_FILENO, bt_syms[i], len, i, addr, size);
    //full_write(STDERR_FILENO, "\n", 1);
  }
  free(bt_syms);
}
#endif

int getShift(){
  char* v=getenv("MALLOC_INTERPOSE_SHIFT");
  int s=10;
  if(v){
    errno=0;
    s=::strtol(v,0,10);
    if((errno==ERANGE)||(errno==EINVAL)){
      errno=0;
      //fprintf(stderr,"MalInt:%d Size Shift= %d (MALLOC_INTERPOSE_SHIFT)\n",__LINE__,10);
      return 10;//default 1k
    }
    //fprintf(stderr,"MalInt:%d Got size shift= %d (MALLOC_INTERPOSE_SHIFT)\n",__LINE__,s);
  }
  //fprintf(stderr,"MalInt:%d Using Size Shift= %d (MALLOC_INTERPOSE_SHIFT)\n",__LINE__,s);
  return s;
}

int getMaxDepth(){
  char* v=getenv("MALLOC_INTERPOSE_DEPTH");
  int s=10;
  if(v){
    errno=0;
    s=::strtol(v,0,10);
    if((errno==ERANGE)||(errno==EINVAL)){
      errno=0;
      //fprintf(stderr,"MalInt:%d Frame depth limit= %d (MALLOC_INTERPOSE_DEPTH)\n",__LINE__,10);
      return 10;//default 10 frames
    }
    //fprintf(stderr,"MalInt:%d Frame depth limit= %d (MALLOC_INTERPOSE_DEPTH)\n",__LINE__,s);
  }
  //fprintf(stderr,"MalInt:%d Frame depth limit= %d (MALLOC_INTERPOSE_DEPTH)\n",__LINE__,s);
  return s;
}


const char* getOutputFileName(){
  char* v=getenv("MALLOC_INTERPOSE_OUTFILE");
  static char fname[2048];
  if(v){
    //fileN=v;
    char *p=NULL;
    if((p=strstr(v,"%p"))){
      // char buf [20];
      ::memcpy(fname,v,p-v);
      char *c=fname+(p-v);
      int nbytes=snprintf(c,20,"%u",getpid());
      c+=nbytes;
      int vlen=strlen(v);
      if(vlen>(p-v)+2){
	memcpy(c,p+2,vlen-(p+2-v)+1);
      }
      //fileN.replace(p-v,2,buf);
      
    }else{
      ::strncpy(fname,v,2048);
    }
  }else{
    //char buff[100];
    snprintf(fname,2048,"mallocHook.%u.fom",getpid());
  }
  return fname;
}

FILE* getOutputFile(){
  char* v=getenv("MALLOC_INTERPOSE_OUTFILE");
  FILE *ofile=stderr;
  std::string fileN;
  if(v){
    char *p=NULL;
    FILE *tmp=NULL;
    fileN=v;
    if((p=strstr(v,"%p"))){
      char buf [1024];snprintf(buf,1024,"%u",getpid());
      fileN.replace(p-v,2,buf);
      //fprintf(stderr,"MalInt:%d trying outfile=%s (MALLOC_INTERPOSE_OUTFILE)\n",__LINE__,fileN.c_str());
      tmp=fopen(fileN.c_str(),"w+");
      //fprintf(stderr,"fileptr= 0x%x\n",(void*)tmp);
    }else{
      //fprintf(stderr,"MalInt:%d outfile=%s (MALLOC_INTERPOSE_OUTFILE)\n",__LINE__,v);
      tmp=fopen(v,"w+");
      //fprintf(stderr,"fileptr= 0x%x\n",(void*)tmp);
    }
    if(tmp==NULL){
      //fprintf(stderr,"MalInt:%d using stderr (MALLOC_INTERPOSE_OUTFILE)\n",__LINE__);
      errno=0;
      return ofile;
    }else{
      ///fprintf(stderr,"MalInt:%d using file %s (MALLOC_INTERPOSE_OUTFILE)\n",__LINE__,fileN.c_str());
      ofile=tmp;
    }
   
  }else{
    //fprintf(stderr,"MalInt1:%d using stderr (MALLOC_INTERPOSE_OUTFILE)\n",__LINE__);
  }
  return ofile;
}

FOM_mallocHook::Writer* getWriter(){
  char* v=getenv("MALLOC_INTERPOSE_OUTFILE");
  std::string fileN;
  if(v){
    fileN=v;
    char *p=NULL;
    if((p=strstr(v,"%p"))){
      char buf [1024];snprintf(buf,1024,"%u",getpid());
      fileN.replace(p-v,2,buf);
      //fprintf(stderr,"MalInt:%d trying outfile=%s (MALLOC_INTERPOSE_OUTFILE)\n",__LINE__,fileN.c_str());
    }else{
      //fprintf(stderr,"MalInt:%d outfile=%s (MALLOC_INTERPOSE_OUTFILE)\n",__LINE__,v);
    }
  }else{
    char buff[1024];
    snprintf(buff,1024,"mallocHook.%u.fom",getpid());
    fileN=buff;
  }
  FOM_mallocHook::Writer *w=new FOM_mallocHook::Writer(fileN);
  return w;
}

void* malloc(size_t size){
  static void* (*func)(size_t)=0;
  static size_t sizeLimit=0;
  static int maxDepth=0;
  void* ret;
  if (!func) {
    func=(void*(*)(size_t))dlsym(RTLD_NEXT,"malloc");
    sizeLimit=(8ul<<getShift());
    maxDepth=getMaxDepth();
    if(!initializedForkHooks.test_and_set()){
      int retVal=pthread_atfork(&prepFork,&postForkParent,&postForkChildren);
      if(retVal!=0){
	std::cerr<<"forking handler registrations failed. If process is forking, sampling may not work."<<std::endl;
      }
    }

    if(maxDepth*sizeof(FOM_mallocHook::index_t)>maxBuff-sizeof(FOM_mallocHook::header)){
      int maxAvailDepth=(maxBuff-sizeof(FOM_mallocHook::header))/sizeof(FOM_mallocHook::index_t)-1;
      std::cerr<<"Max stack depth is too high, please recompile with increased maxBuff. Limiting max stack depth to "<<maxAvailDepth<<std::endl;
      maxDepth=maxAvailDepth;
    }
  }
  if(!fwriter){
    if(!malloc_tracing_flag_sami.test_and_set()){
      std::atexit(atexit_handler);
      fwriter=getWriter();
      sizeLimit=(8ul<<getShift());
      maxDepth=getMaxDepth();
      if(maxDepth*sizeof(FOM_mallocHook::index_t)>maxBuff-sizeof(FOM_mallocHook::header)){
	int maxAvailDepth=(maxBuff-sizeof(FOM_mallocHook::header))/sizeof(FOM_mallocHook::index_t)-1;
	std::cerr<<"Max stack depth is too high, please recompile with increased maxBuff. Limiting max stack depth to "<<maxAvailDepth<<std::endl;
	maxDepth=maxAvailDepth;
      }
      malloc_tracing_flag_sami.clear();
      //std::cerr<<__PRETTY_FUNCTION__<<" Created writer"<<std::endl;
    }else{
      ret=func(size);
      return ret;
    }
  }
  
  ret=func(size);
  if((size>=sizeLimit) && !malloc_tracing_flag_sami.test_and_set()){
#ifndef __DO_GNU_BACKTRACE__
    show_backtrace(0,size,ret,maxDepth,1);
#else
    show_backtraceGNU(0,size,ret,maxDepth);
#endif
    malloc_tracing_flag_sami.clear();
  }
  return ret;
}

void* realloc(void *ptr, size_t size) throw(){
  static void* (*func)(void*,size_t)=0;
  static size_t sizeLimit=0;
  static int maxDepth=0;
  void* ret;
  if (!func) {
    func=(void*(*)(void*, size_t))dlsym(RTLD_NEXT,"realloc");
    sizeLimit=(8ul<<getShift());
    maxDepth=getMaxDepth();
    if(maxDepth*sizeof(FOM_mallocHook::index_t)>maxBuff-sizeof(FOM_mallocHook::header)){
      int maxAvailDepth=(maxBuff-sizeof(FOM_mallocHook::header))/sizeof(FOM_mallocHook::index_t)-1;
      std::cerr<<"Max stack depth is too high, please recompile with increased maxBuff. Limiting max stack depth to "<<maxAvailDepth<<std::endl;
      maxDepth=maxAvailDepth;
    }
    //if(!mhbuildInfo)mhbuildInfo=new FOM_mallocHook::MallocBuildInfo(__PRETTY_FUNCTION__);
  }
  if(!fwriter){
    if(!malloc_tracing_flag_sami.test_and_set()){
      std::atexit(atexit_handler);
      fwriter=getWriter();
      sizeLimit=(8ul<<getShift());
      maxDepth=getMaxDepth();
      if(maxDepth*sizeof(FOM_mallocHook::index_t)>maxBuff-sizeof(FOM_mallocHook::header)){
	int maxAvailDepth=(maxBuff-sizeof(FOM_mallocHook::header))/sizeof(FOM_mallocHook::index_t)-1;
	std::cerr<<"Max stack depth is too high, please recompile with increased maxBuff. Limiting max stack depth to "<<maxAvailDepth<<std::endl;
	maxDepth=maxAvailDepth;
      }
      malloc_tracing_flag_sami.clear();
      //std::cerr<<__PRETTY_FUNCTION__<<"Created writer"<<std::endl;

    }else{
      ret=func(ptr,size);
      return ret;
    }
  }
  ret=func(ptr, size);
  if((size>=sizeLimit) && !malloc_tracing_flag_sami.test_and_set()){
#ifndef __DO_GNU_BACKTRACE__
    show_backtrace(0,size,ret,maxDepth,2);
#else
    show_backtraceGNU(0,size,ret,maxDepth);
#endif
    malloc_tracing_flag_sami.clear();
  }
  return ret;
}

void* calloc(size_t nobj, size_t size) {
  static void* (*func)(size_t,size_t)=0;
  static size_t sizeLimit=0;
  static int maxDepth=0;
  void* ret;
  if (!func) {
    if(!calloc_tracing_flag_sami.test_and_set()){
      func=(void*(*)(size_t, size_t))dlsym(RTLD_NEXT,"calloc");
    }else{
      if((4096-dlsymBuffPos)>(nobj*size)){
	ret=(void*)(dlsymBuff()+dlsymBuffPos);
	dlsymBuffPos+=nobj*size;
	char* tmp=(char*)ret;
	for(size_t t=0;t<nobj*size;t++){
	  *tmp=0;
	}
      }else{
	ret=0;
	std::cerr<<__PRETTY_FUNCTION__<<" Returning NULL"<<std::endl;
      }
      calloc_tracing_flag_sami.clear();
      return ret;
    }
  }
  if(!fwriter){
    if(!malloc_tracing_flag_sami.test_and_set()){
      std::atexit(atexit_handler);
      fwriter=getWriter();
      sizeLimit=(8ul<<getShift());
      maxDepth=getMaxDepth();
      if(maxDepth*sizeof(FOM_mallocHook::index_t)>maxBuff-sizeof(FOM_mallocHook::header)){
	int maxAvailDepth=(maxBuff-sizeof(FOM_mallocHook::header))/sizeof(FOM_mallocHook::index_t)-1;
	std::cerr<<"Max stack depth is too high, please recompile with increased maxBuff. Limiting max stack depth to "<<maxAvailDepth<<std::endl;
	maxDepth=maxAvailDepth;
      }
      malloc_tracing_flag_sami.clear();
      //std::cerr<<__PRETTY_FUNCTION__<<"Created writer"<<std::endl;
    }else{
      ret=func(nobj,size);
      return ret;
    }
  }
  ret=func(nobj, size);
  if((nobj*size>=sizeLimit) && !malloc_tracing_flag_sami.test_and_set()){
#ifndef __DO_GNU_BACKTRACE__
    show_backtrace(0,nobj*size,ret,maxDepth,3);
#else
    show_backtraceGNU(0,nobj*size,ret,maxDepth);
#endif
    malloc_tracing_flag_sami.clear();
  }
  return ret;
}
