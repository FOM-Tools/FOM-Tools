#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <time.h>
#include <unistd.h>

#include "gtest/gtest.h"

bool test(size_t t){
  bool success(true);
  std::cout<<"Trying to allocate memory for "<<t<<" doubles"<<std::endl;
  double *p=0;
  if(t>0){
    p=new double[t];
  }else{
    return false;
  }
  uint64_t asd=1;
  asd=asd<<63;

  p[0]=23.;
  p[t-1]=23;
  //alloc all pages in memory
  for(size_t i=0;i<t;i++){
    p[i]=3.1415926;
  }
  int max=100000;
  if(max>t)max=t;
  for(int i=0;i<max;i++){
    p[0]=i*3.1415926;
    p[i]=i*3.1415926;
  }
  std::string s;
  std::cout<<"Allocated "<<t<<" doubles at "<<std::hex
	   <<(void*)p<<std::dec<<std::endl;
  std::cout<<"Trying calloc ";
  delete[] p;
  void *dummMem=calloc(sizeof(double),t);
  if(dummMem!=0){
    std::cout<<" Succeeded @ "<<dummMem<<" with size "<<t<<std::endl;
  }else{
    success = false;
    std::cout<<" Failed!"<<std::endl;
  }
  std::cout<<"Trying realloc ";
  dummMem=realloc(dummMem,(t+t/2)*sizeof(double));
  if(dummMem!=0){
    std::cout<<" Succeeded @ "<<dummMem<<" with size "<<t+t/2<<std::endl;
  }else{
    std::cout<<" Failed!"<<std::endl;
    success = false;
  }
  free(dummMem);
  return success;
}

TEST(FOMTools, malloc_hook) {
  //void testHook(){
  bool success(true);
  std::vector<int> vals{1,5,6,8,9,10,11,12,14};
  for(size_t i=0;i<vals.size();i++){
    std::cout<<"i is "<<vals[i]<<" t is " <<(4096l<<vals[i])<<std::endl;
    success = success && test(4096l<<vals[i]);
  }
  pid_t t=fork();
  if(t==0){//children
    for(size_t i=vals.size()-1;i>0;i--){
      std::cout<<" l i="<<i<<" i is "<<vals[i]<<" t is " <<(4096l<<vals[i])<<" Children "<<std::endl;
      success = success && test(4096l<<vals[i]);
    }

  }else{//mother;
    for(size_t i=0;i<vals.size();i++){
      std::cout<<"i is "<<vals[i]<<" t is " <<(4096l<<vals[i])<<" mother"<<std::endl;
      success = success && test(4096l<<vals[i]);
    }
 }
  int rc;
  struct timespec res;

  rc = clock_getres(CLOCK_MONOTONIC, &res);
  if (!rc)
    printf("CLOCK_MONOTONIC: %ldns\n", res.tv_nsec);
  rc = clock_getres(CLOCK_MONOTONIC_COARSE, &res);
  if (!rc)
    printf("CLOCK_MONOTONIC_COARSE: %ldns\n", res.tv_nsec);
  std::string s;
  EXPECT_TRUE(success);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
  //testHook();
}
