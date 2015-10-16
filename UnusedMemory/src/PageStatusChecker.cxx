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

/* Read pagemap of a given process, iterate over given page range and obtain status. 
   Write output file with the following format:
   Iteration addressHex addressInt Bit63+Bit62 Bit61+Bit55 */

#include <Python.h>
#include <fstream>
#include <fcntl.h>
#include <ctime>

#define PAGEMAP_INFOFIELD 8

int fileDescriptor;
unsigned long adr, vma, vmaEnd;
uint64_t readValue, fileOffset,fileEnd;
char pathBuffer [0x100] = {};
unsigned long pagesize = getpagesize();
static double msecResolution = 1.0/1000000;

static PyObject* analyze(PyObject * self, PyObject * args){ 
   
   int pid; char* tmp1 = NULL; char* tmp2 = NULL; int iteration; char* filename;

   if (!PyArg_ParseTuple(args, "iisss", &pid, &iteration, &filename, &tmp1, &tmp2)) return NULL;

   vma = strtol(tmp1, NULL, 16);
   vmaEnd = strtol(tmp2, NULL, 16);
   std::ofstream outputFile;
   outputFile.open(filename); 

   if(pid != -1)
      sprintf(pathBuffer, "/proc/%d/pagemap", pid);
   fileDescriptor = open(pathBuffer,O_RDONLY);

   if(fileDescriptor < 0){
      printf("Error! Cannot open %s\t%lu\n", pathBuffer, adr);
      return PyErr_Occurred();
   }
 
   adr = vma; 
   fileOffset  = adr / pagesize * PAGEMAP_INFOFIELD;
   off_t seekOffset = lseek(fileDescriptor, fileOffset, SEEK_SET);
   fileEnd  = vmaEnd / pagesize * PAGEMAP_INFOFIELD;
   adr = (fileOffset/PAGEMAP_INFOFIELD) * pagesize;
   char buffer[300];

   struct timespec tp;
   int rc = 0;
   rc=clock_gettime(CLOCK_MONOTONIC_COARSE,&tp);
   snprintf(buffer, 300, "%ld.%03ld\n", (long)tp.tv_sec,
	    (long)(tp.tv_nsec * msecResolution));
   outputFile << buffer;

   while (fileOffset < fileEnd){

     readValue = 0;
        
     if(read(fileDescriptor, &readValue, sizeof(readValue)) != sizeof(readValue))
         return PyErr_Occurred();

     snprintf(buffer, 300, "%d\t0x%lx\t%lu\t%1d%1d\t%1d%1d\n",iteration,adr,adr,(readValue&9223372036854775808ull)>0,(readValue&4611686018427387904ull)>0,(readValue&2305843009213693952ull)>0,(readValue&36028797018963968ull)>0);
     outputFile<<buffer;
     fileOffset += sizeof(readValue);
     adr = adr + pagesize;
   }

close(fileDescriptor);
outputFile.close();

return Py_None;
}

static PyMethodDef methods[]= {{(char *)"analyze", (PyCFunction)analyze, METH_VARARGS, NULL},{ NULL, NULL, 0, NULL }};

PyMODINIT_FUNC initPageStatusChecker(void) { Py_InitModule("PageStatusChecker", methods); }

