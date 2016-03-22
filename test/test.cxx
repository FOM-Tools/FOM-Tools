#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <random>
#include <functional>

void printUsage(char* name){
  std::cout<<"Usage:  "<<name<<" -r <num>  "<<std::endl;
  std::cout<<"     --random  (-r)  number of random allocation and free attemps"<<std::endl;
  std::cout<<"     --leaks (-l)  Emulate leaks at the end"<<std::endl;
}

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

void testHook(){
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
}

void runRandomAllocs(size_t nRand,bool leaks){

  std::default_random_engine typeEng,posEng,sizeEng;
  typeEng.seed(1234);
  posEng.seed(1234);
  sizeEng.seed(1234);
  std::uniform_int_distribution<int> allocDist(0,3);
  std::uniform_int_distribution<int> sizeDist(3,13);
  std::uniform_real_distribution<double> posDist(0,1);
  auto typeDice = std::bind ( allocDist, typeEng );
  auto posDice = std::bind ( posDist, posEng );
  auto sizeDice = std::bind ( sizeDist, sizeEng );
  //std::vector<int> vals{1,5,6,8,9,10,11,12,14};
  std::vector<char*> memLocations;
  std::vector<std::string> opName{"free","malloc","realloc","calloc"};
  size_t s=1<<sizeDice();
  char* v=(char*)malloc(s);
  printf("%s %x size= %d\n","malloc",v,s);
  memLocations.push_back(v);
  for(size_t t=0;t<nRand;t++){
    int aType=typeDice();
    //printf("Type is %d ",aType);
    switch(aType){
    case 0://free
      {
	if(memLocations.size()){
	  size_t loc=posDice()*(memLocations.size()-1);
	  v=memLocations[loc];
	  memLocations.erase(memLocations.begin()+loc);
	  printf("%ld %s %x\n",t,opName[aType].c_str(),v);
	  free(v);
	}else{
	  t--;
	  continue;
	}
	break;
      }
    case 1://malloc
      {
	s=1<<sizeDice();
	v=(char*)malloc(s);
	memLocations.push_back(v);
	printf("%ld %s %x size= %d\n",t,opName[aType].c_str(),v,s);	
	break;
      }
    case 2://realloc
      {
	if(memLocations.size()){
	  size_t loc=posDice()*(memLocations.size()-1);
	  v=memLocations[loc];
	  memLocations.erase(memLocations.begin()+loc);
	  s=1<<sizeDice();
	  char *n=(char*)realloc(v,s);
	  memLocations.push_back(n);
	  printf("%ld %s old=%x new=%x size= %d\n",t,opName[aType].c_str(),v,n,s);
	}else{
	  t--;
	  continue;
	}
	break;
      }
    case 3://calloc
      {
	s=1<<sizeDice();
	v=(char*)malloc(s);
	memLocations.push_back(v);
	printf("%ld %s %x size= %d\n",t,opName[aType].c_str(),v,s);	
	break;
      }
    default:
      {
	t--;
	break;
      }
    }
  }
  if(leaks){
    if(memLocations.size()){
      size_t toFree=posDice()*(memLocations.size()-1);
      for(size_t t=0;t<toFree;t++){
	size_t loc=posDice()*(memLocations.size()-1);
	free(memLocations[loc]);
	printf("free %x\n",memLocations[loc]);
	memLocations.erase(memLocations.begin()+loc);
      }
    }
    for(size_t t=0;t<memLocations.size();t++){
      printf("leak %x\n",memLocations[t]);
    }
  }else{
    for(size_t t=0;t<memLocations.size();t++){
      size_t loc=posDice()*(memLocations.size()-1);
      free(memLocations[loc]);
      printf("free %x\n",memLocations[loc]);
      memLocations.erase(memLocations.begin()+loc);
    }
  }
}

int main(int argc, char **argv) {
  int c;
  size_t nRandom=0;
  bool leaks=false;
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
      {"help", 0, 0, 'h'},
      {"random", 1, 0, 'r'},
      {"leaks", 0, 0, 'l'},
      {0, 0, 0, 0}
    };
    c = getopt_long(argc, argv, "hr:l",
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 'h':
      printUsage(argv[0]);
      exit(EXIT_SUCCESS);
      break;
    case 'r':  {
      nRandom=std::strtoul(optarg,0,10);
      break;
    }
    case 'l':  {
      leaks=true;
      break;
    }
    default:
      printf("unknown parameter! getopt returned character code 0%o ??\n", c);
    }
  }
  pid_t p=getpid();
  testHook();
  if(p!=getpid())return 0;
  if(nRandom>0){
    runRandomAllocs(nRandom,leaks);
  }
}
