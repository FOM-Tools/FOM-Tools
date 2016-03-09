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
static bool captureEnabled = true;

#define __MAXBUFF__ 4096
static int maxBuff=__MAXBUFF__;
static unsigned long int  counter = 0;
char* fileBuff(){
  static char fileBuff[__MAXBUFF__];
  return fileBuff;
}
struct timespec tp;
int rc=clock_gettime(CLOCK_MONOTONIC,&tp);
static long starttime = (long)tp.tv_sec;

static FOM_mallocHook::Writer* fwriter=0;
FOM_mallocHook::Writer*& currWriter(FOM_mallocHook::Writer* w){
  static FOM_mallocHook::Writer* wLocal=0;
  if(w){
    wLocal=w;
    //std::cerr<<"Setting wLocal to "<<(void*)w<<" curr value="<<(void*)wLocal<<std::endl;
  }
  //std::cerr<<" currWriter returning="<<(void*)wLocal<<" @pid="<<getpid()<<std::endl;
  return wLocal;
}

static int dlsymBuffPos=0;


extern "C" {
  void* malloc(size_t size) throw();
  void* realloc(void* ptr,size_t size) throw();
  void* calloc(size_t n,size_t s) throw();
  void free(void* ptr);
  bool mallocHookSetCapture(bool b);
}

bool  mallocHookSetCapture(bool b){
  bool old(captureEnabled);
  captureEnabled=b;
  return old;
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
  FOM_mallocHook::Writer*& FWriter(currWriter(0));
  //std::cerr<<__PRETTY_FUNCTION__<<" @pid "<<getpid()<<std::endl;
  if(!FWriter){
    //std::cerr<<"writer is 0 @pid="<<getpid()<<std::endl;
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
  //std::cerr << "Counter " << counter << std::endl;
  delete FWriter;
  FWriter=0;
  delete mhbuildInfo;
  mhbuildInfo=0;
  delete &symNames();
  delete &symMap();
}

void prepFork(){
  if(fwriter){
    //if(!malloc_tracing_flag_sami.test_and_set()){
    while(malloc_tracing_flag_sami.test_and_set(std::memory_order_acquire));//spin until get the lock
    fwriter->closeFile(false);
    //malloc_tracing_flag_sami.clear();
    malloc_tracing_flag_sami.clear(std::memory_order_release);
    //}
  }
}

void postForkParent(){
  if(fwriter){
    while(malloc_tracing_flag_sami.test_and_set(std::memory_order_acquire));//spin until get the lock
    fwriter->reopenFile(true);
    malloc_tracing_flag_sami.clear(std::memory_order_release);
  }
}

void postForkChildren(){
  if(fwriter){
    while(malloc_tracing_flag_sami.test_and_set(std::memory_order_acquire));//spin until get the lock
    //if(!malloc_tracing_flag_sami.test_and_set()){
    delete fwriter;
    fwriter=getWriter();
    currWriter(fwriter);
    malloc_tracing_flag_sami.clear(std::memory_order_release);
    //}
  }
  //std::cout<<"Called postForkChildren @ pid="<<getpid()<<std::endl;
  std::atexit(atexit_handler);
}

void show_backtrace (size_t size,void* addr,int depth,int allocType, uint64_t t1, uint64_t t2, void* ra_addr) {
  int count=0;
  //  struct timespec tp;
  //  int rc=clock_gettime(CLOCK_MONOTONIC,&tp);
  FOM_mallocHook::header *hdr=(FOM_mallocHook::header*)(fileBuff());
  hdr->tstart = t1;                  //time sec
  hdr->treturn = t2;
  hdr->size=size;                       //size of allocation
  FOM_mallocHook::index_t *stackRecord=(FOM_mallocHook::index_t*)(hdr+1);
  //if (t1.tv_sec-starttime > 1000 && addr != 0 && size > 0){ //skip init time with malloc hook
  if (addr != 0 && size > 0){
    unw_cursor_t cursor; unw_context_t uc;
    unw_word_t ip;
    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);
    if(symMap().size()>=std::numeric_limits<FOM_mallocHook::index_t>::max()){
      std::cerr<<" Malloc hook has reached its indexing capacity. Please recompile with a wider index_t. Aborting!"<<std::endl;
      std::abort();
    }
    while (unw_step(&cursor) > 0 && count<depth) {
      unw_get_reg(&cursor, UNW_REG_IP, &ip);
      auto it=symMap().insert(std::make_pair(ip,symMap().size()));
      //fprintf(stderr," ip=%ld\n",(long)ip);
      if(it.second){// new IP
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
  }
  struct timespec t3;
  int rc=clock_gettime(CLOCK_MONOTONIC,&t3);
  if(allocType==2){//realloc write fake free first
    hdr->count=0;
    hdr->addr=(uintptr_t)ra_addr;
    hdr->allocType=0;
    hdr->treturn=t1;
    hdr->tend=t1;
    fwriter->writeRecord(hdr);
    hdr->tstart=t1+1;
    hdr->treturn=t2;
  }
  hdr->addr=(uintptr_t)addr;                       //returned addres
  hdr->count=count;
  hdr->allocType=(char)allocType;
  rc=clock_gettime(CLOCK_MONOTONIC,&t3);
  hdr->tend = t3.tv_sec*1000000000l+t3.tv_nsec;
  fwriter->writeRecord(hdr);
  counter = counter + 1;
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
  //return 20;
  char* v=getenv("MALLOC_INTERPOSE_DEPTH");
  int s=10;
  if(v){
    errno=0;
    s=::strtol(v,0,10);
    if((errno==ERANGE)||(errno==EINVAL)){
      errno=0;
      //fprintf(stderr,"MalInt:%d Frame depth limit= %d (MALLOC_INTERPOSE_DEPTH)\n",__LINE__,10);
      return 20;//default 20 frames
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
  char *com=getenv("MALLOC_INTERPOSE_COMPRESSION");
  int compress=0;
  if(com){
    char* end;
    compress=std::strtol(com,&end,10);
  }
  size_t bucketSize=65536;
  char *buck=getenv("MALLOC_INTERPOSE_BUCKET_SIZE");
  if(com){
    char* end;
    bucketSize=std::strtoull(buck,&end,10);
  }
  FOM_mallocHook::Writer *w=new FOM_mallocHook::Writer(fileN,compress,bucketSize);
  return w;
}

void* malloc(size_t size) throw() {
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
      currWriter(fwriter);
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
  struct timespec t1;
  struct timespec t2;
  clock_gettime(CLOCK_MONOTONIC,&t1);
  ret=func(size);
  clock_gettime(CLOCK_MONOTONIC,&t2);
  
  //  if((size>=sizeLimit) && !malloc_tracing_flag_sami.test_and_set()){
  if( 
#ifdef ENABLE_USER_CONTROL
     captureEnabled &&
#endif
     !malloc_tracing_flag_sami.test_and_set()
      ){
#ifndef __DO_GNU_BACKTRACE__
    show_backtrace(size,ret,maxDepth,1, 
		   t1.tv_sec*1000000000l+t1.tv_nsec, 
		   t2.tv_sec*1000000000l+t2.tv_nsec,0);
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
      currWriter(fwriter);
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
  struct timespec t1;
  struct timespec t2;
  clock_gettime(CLOCK_MONOTONIC,&t1);
  ret=func(ptr, size);
  clock_gettime(CLOCK_MONOTONIC,&t2);

  //  if((size>=sizeLimit) && !malloc_tracing_flag_sami.test_and_set()){
  if( 
#ifdef ENABLE_USER_CONTROL
     captureEnabled &&
#endif
     !malloc_tracing_flag_sami.test_and_set()
      ){
#ifndef __DO_GNU_BACKTRACE__
    show_backtrace(size,ret,maxDepth,2,
		   t1.tv_sec*1000000000l+t1.tv_nsec, 
		   t2.tv_sec*1000000000l+t2.tv_nsec,ptr);
#else
    show_backtraceGNU(0,size,ret,maxDepth);
#endif
  malloc_tracing_flag_sami.clear();
  }
  return ret;
}

void* calloc(size_t nobj, size_t size) throw() {
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
      currWriter(fwriter);
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
  struct timespec t1;
  struct timespec t2;  
  clock_gettime(CLOCK_MONOTONIC,&t1);
  ret=func(nobj, size);
  clock_gettime(CLOCK_MONOTONIC,&t2);

  //  if((nobj*size>=sizeLimit) && !malloc_tracing_flag_sami.test_and_set()){
  if( 
#ifdef ENABLE_USER_CONTROL
     captureEnabled &&
#endif
     !malloc_tracing_flag_sami.test_and_set()
      ){
#ifndef __DO_GNU_BACKTRACE__
    show_backtrace(nobj*size,ret,maxDepth,3, 
		   t1.tv_sec*1000000000l+t1.tv_nsec, 
		   t2.tv_sec*1000000000l+t2.tv_nsec,0);
#else
    show_backtraceGNU(0,nobj*size,ret,maxDepth);
#endif
    malloc_tracing_flag_sami.clear();
  }
  return ret;
}

void free (void *ptr){	
  static void (*func) (void*) = 0;
  static size_t sizeLimit=0;
  static int maxDepth=0;

  if (! func){ 
    func = (void (*) (void*)) dlsym (RTLD_NEXT, "free");
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
      currWriter(fwriter);
      sizeLimit=(8ul<<getShift());
      maxDepth=getMaxDepth();
      if(maxDepth*sizeof(FOM_mallocHook::index_t)>maxBuff-sizeof(FOM_mallocHook::header)){
	int maxAvailDepth=(maxBuff-sizeof(FOM_mallocHook::header))/sizeof(FOM_mallocHook::index_t)-1;
	std::cerr<<"Max stack depth is too high, please recompile with increased maxBuff. Limiting max stack depth to "<<maxAvailDepth<<std::endl;
	maxDepth=maxAvailDepth;
      }
      malloc_tracing_flag_sami.clear();
    }else{
      func(ptr);
      return;
    }
  }

  struct timespec t1;
  struct timespec t2;
  clock_gettime(CLOCK_MONOTONIC,&t1);
  func(ptr);
  clock_gettime(CLOCK_MONOTONIC,&t2);
  //if((nobj*size>=sizeLimit) && !malloc_tracing_flag_sami.test_and_set()){


  if ( 
#ifdef ENABLE_USER_CONTROL
      captureEnabled &&
#endif
      !malloc_tracing_flag_sami.test_and_set()){
#ifndef __DO_GNU_BACKTRACE__
    show_backtrace(0,ptr,maxDepth,0, 
		   t1.tv_sec*1000000000l+t1.tv_nsec, 
		   t2.tv_sec*1000000000l+t2.tv_nsec,0);
#else
    show_backtraceGNU(0,size,ret,maxDepth);
#endif
    malloc_tracing_flag_sami.clear();
  }
}
