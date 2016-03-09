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

/*
 * Implements the Python layer for:
 *   reading merged and unmerged output files into Numpy arrays 
 *   freeing allocated arrays
 *   calling RegionFinder in order to find objects for a given page range
 *   opening and closing malloc output file
 */

#include <Python.h>
#include "numpy/arrayobject.h"
#include <cstdint>
#include "FOMTools/Streamers.hpp"
#include "FOMTools/MergePages.hpp"
#include "FOMTools/Parser.hpp"
#include "FOMTools/RegionFinder.hpp"
#include "FOMTools/Addr2Line.hpp"

static PyObject* PyMergePages(PyObject* self, PyObject* args)
{
  char* inputFilename;
  char* outputFilename;
  if (!PyArg_ParseTuple(args, "ss", &inputFilename, &outputFilename)) return NULL;
  MergePages(inputFilename, outputFilename);

  Py_INCREF(Py_None);
  return Py_None;
}    


static PyObject* PyParseOutputFiles(PyObject * self, PyObject * args)
{
  char* iterationFilename = NULL;
  int begin = -1;
  int end = -1;

  if (!PyArg_ParseTuple(args, "s|ii", &iterationFilename, &begin, &end)) return NULL;
  Array p = parseOutputFiles(iterationFilename, begin, end);

  int tmp = p.size/sizeof(uint64_t);
  npy_intp dim[1];
  dim[0] = tmp; 
  PyArrayObject  *c = (PyArrayObject*)  PyArray_SimpleNewFromData(1, dim, NPY_UINT64, (void*) p.pointer);
  c->flags = NPY_C_CONTIGUOUS | NPY_WRITEABLE | NPY_OWNDATA;

  return PyArray_Return(c);
}

static PyObject* PyParseMergedOutputFiles(PyObject * self, PyObject * args)
{
  char* iterationFilename = NULL;
  if (!PyArg_ParseTuple(args, "s", &iterationFilename)) return NULL;
  Array p = parseMergedOutputFiles(iterationFilename);

  int tmp = p.size/sizeof(uint64_t);
  npy_intp dim[1];
  dim[0] = tmp;
  PyArrayObject  *c = (PyArrayObject*)  PyArray_SimpleNewFromData(1, dim, NPY_UINT64, (void*) p.pointer);
  c->flags = NPY_C_CONTIGUOUS | NPY_WRITEABLE | NPY_OWNDATA;
  return PyArray_Return(c);
}

static FOM_mallocHook::RegionFinder *finder=0;

namespace FOMPython{
  static FOM_mallocHook::IndexingReader *s_InReader=0;
  static size_t sIRdrCurrOffset=0;
};

static PyObject* PyOpenMallocFile(PyObject* self, PyObject* args)
{
  char* inputFilename;
  if (!PyArg_ParseTuple(args, "s", &inputFilename)) return NULL;
  delete finder;
  finder = new FOM_mallocHook::RegionFinder(inputFilename);

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject* PyCloseMallocFile(PyObject* self, PyObject* args)
{
  delete finder;
  finder = 0;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* PyOpenFileAsStream(PyObject* self, PyObject* args)
{
  char* inputFilename;
  if (!PyArg_ParseTuple(args, "s", &inputFilename)) return NULL;
  delete FOMPython::s_InReader;
  FOMPython::s_InReader = new FOM_mallocHook::IndexingReader(inputFilename);
  // Py_INCREF(Py_None);
  // return Py_None;
  return Py_BuildValue("K",FOMPython::s_InReader->size());
}

static PyObject* PyCloseFileAsStream(PyObject* self, PyObject* args)
{
  delete FOMPython::s_InReader;
  FOMPython::s_InReader = 0;
  Py_INCREF(Py_None);
  return Py_None;
}



static PyObject* PyGetAllocationTraces(PyObject *self, PyObject * args)
{
  if (!finder){
    PyErr_SetString(PyExc_RuntimeError, "Open first malloc output file");
    return NULL;}

  int time;
  PyObject* b;
  PyObject* e;
  char* filename;
  char* mallocSourcelinesFilename;

  if (!PyArg_ParseTuple(args, "OOss|i", &b, &e, &filename, &mallocSourcelinesFilename, &time)) return NULL;

  PyObject *b2 = NULL; 
  b2 = PyArray_FROM_OTF(b, NPY_UINT64, NPY_IN_ARRAY);
  PyObject *e2 = NULL; 
  e2 = PyArray_FROM_OTF(e, NPY_UINT64, NPY_IN_ARRAY);

  if (b2 != NULL && e2 != NULL){
    unsigned long *begin = (unsigned long*) PyArray_DATA(b2);
    unsigned long *end = (unsigned long*) PyArray_DATA(e2);

    std::cout << std::hex << "0x" << begin[0] << " to 0x" << end[0] << std::endl;

    RegionInfo r; 
    r.pBegin = begin[0];
    r.pEnd   = end[0];
    r.alloc_time  = time;
    std::ofstream outputFile;
    outputFile.open(filename); 
    char buffer[300];
    std::vector<FOM_mallocHook::MemRecord> result = finder->getAllocations(r);   

    std::ifstream mallocSourcelinesFile;
    mallocSourcelinesFile.open(mallocSourcelinesFilename);  
    std::string line;
    std::vector<unsigned int> stackIDs;
    std::vector<std::string> sourcelines;
    while (std::getline(mallocSourcelinesFile, line, '\n')){
      int i = 0;
      std::string s;
      std::stringstream ss(line);
      while (getline(ss, s, '\t')){
        if (i==0) stackIDs.push_back(atoi(s.c_str())); 
        if (i==2) sourcelines.push_back(s.c_str()); 
        i = i + 1; 
      }
    }
    mallocSourcelinesFile.close();

    for(size_t i=0; i<result.size(); i++){
      snprintf(buffer, 300, "\nFrom 0x%lx to 0x%lx Size %lu kB at 0x%lx (%lu Bytes)\t",  result[i].getFirstPage(), result[i].getLastPage(), ((result[i].getLastPage()-result[i].getFirstPage())>>10), result[i].getAddr(), result[i].getSize());
      outputFile << buffer;
      auto t = result[i].getStacks();
      for(size_t l=0; l < t.size(); l++ ){
	for(int k=0; k < stackIDs.size(); k++){
	  if (t[l] == stackIDs[k]){
	    snprintf(buffer, 300, "\n%d\t%s", t[l], sourcelines[k].c_str());
	    outputFile << buffer;     
	    break;
	  }
	}
      }
      snprintf(buffer, 300, "\n");
      outputFile << buffer; 
    }
    outputFile.close();
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* PyGetObjectValidity(PyObject *self, PyObject * args)
{
  if (!finder){
    PyErr_SetString(PyExc_RuntimeError, "Open first malloc output file");
    return NULL;}

  PyObject* b;
  PyObject* e;
  PyObject* t;
  PyObject* a;

  unsigned long addr;
  int size;
  if (!PyArg_ParseTuple(args, "OOOO", &b, &e, &t, &a)) Py_RETURN_NONE;

  if (b != NULL && e != NULL && t != NULL && a != NULL){
    PyObject *b2 = NULL;
    PyObject *e2 = NULL;
    PyObject *t2 = NULL;
    PyObject *a2 = NULL;

    b2 = PyArray_FROM_OTF(b, NPY_UINT64, NPY_IN_ARRAY);
    e2 = PyArray_FROM_OTF(e, NPY_UINT64, NPY_IN_ARRAY);
    t2 = PyArray_FROM_OTF(t, NPY_UINT64, NPY_IN_ARRAY);
    a2 = PyArray_FROM_OTF(a, NPY_UINT64, NPY_IN_ARRAY);

    size_t nb = PyArray_DIM(b2,0);
    size_t ne = PyArray_DIM(e2,0);
    size_t nt = PyArray_DIM(t2,0);
    size_t na = PyArray_DIM(a2,0);

    unsigned long *b3 = (unsigned long*) PyArray_DATA(b2);
    unsigned long *e3 = (unsigned long*) PyArray_DATA(e2);
    uint64_t *t3      = (uint64_t*) PyArray_DATA(t2);
    unsigned long *a3 = (unsigned long*) PyArray_DATA(a2);
   
    if (!(nb == ne && nb == nt)){
      Py_INCREF(Py_None); 
      return Py_None;
    }
    std::vector<RegionInfo> regions(nb);
    
    for(size_t i = 0; i < nb; i++){
      regions[i].pBegin = b3[i];
      regions[i].pEnd = e3[i];
      regions[i].alloc_time = t3[i];     
    }
    
    std::vector<std::vector<FOM_mallocHook::MemRecord>> result = finder->getAllocationSets(regions);
    
    npy_intp dim[1];
    dim[0] = nb;
    uint64_t* p = (uint64_t*) malloc(sizeof(uint64_t)*nb);
    PyArrayObject  *c = (PyArrayObject*)  PyArray_SimpleNewFromData(1, dim, NPY_UINT64, (void*) p);
    c->flags = (NPY_C_CONTIGUOUS | NPY_WRITEABLE | NPY_OWNDATA);
    
    for(size_t l=0; l<result.size();l++){ 
      p[l] = 0;
      for(size_t i=0; i<result[l].size(); i++){
        const auto &res=result[l][i];
        //double rtime=res.getTimeSec()+res.getTimeNSec()*1.e-9;
        //if ((rtime-t3[l])>0){
        if ((res.getTStart()-t3[l])>0){
	  if((res.getAddr() <= a3[l]) && a3[l] < (res.getAddr() + res.getSize())){
	    p[l] = res.getTStart();
            break;
	  }
	}
      }
    }  
    return PyArray_Return(c); 
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* PyAddress2Line(PyObject *self, PyObject * args){

  char* symbolLookupTable;
  char* maps;
  char* outputFile;

  if (!PyArg_ParseTuple(args, "sss", &symbolLookupTable, &maps, &outputFile)) return NULL;
 
  translate(symbolLookupTable, maps, outputFile);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* PyGetNextFromSteam(PyObject *self, PyObject * args){
  if(FOMPython::s_InReader==0){
    PyErr_SetString(PyExc_RuntimeError, "Streaming file was not opened");
    return NULL;
  }
  if(FOMPython::sIRdrCurrOffset>=FOMPython::s_InReader->size()){
    Py_INCREF(Py_None);
    return Py_None;
  }
  FOMPython::sIRdrCurrOffset++;
  auto r = FOMPython::s_InReader->at(FOMPython::sIRdrCurrOffset);
  if(!r.getHeader()){
    Py_INCREF(Py_None);
    return Py_None;  
  }
  int count=0;
  auto stackArray=r.getStacks(&count);
  PyObject* stackList=PyTuple_New(count);
  for(int i=0;i<count;i++){
    PyTuple_SetItem(stackList,i,PyInt_FromLong(stackArray[i]));
  }
  PyObject* resultList=PyList_New(7);
  PyList_SetItem(resultList,0,Py_BuildValue("K",r.getTStart()));
  PyList_SetItem(resultList,1,Py_BuildValue("K",r.getTReturn()));
  PyList_SetItem(resultList,2,Py_BuildValue("K",r.getTEnd()));
  PyList_SetItem(resultList,3,Py_BuildValue("B",r.getAllocType()));
  PyList_SetItem(resultList,4,Py_BuildValue("K",r.getAddr()));
  PyList_SetItem(resultList,5,Py_BuildValue("K",r.getSize()));
  PyList_SetItem(resultList,6,stackList);
  return resultList;
}

static PyObject* PyNumRecords(PyObject *self, PyObject * args){
  if(FOMPython::s_InReader){
    return Py_BuildValue("K",FOMPython::s_InReader->size());
  }
  PyErr_SetString(PyExc_RuntimeError, "Streaming file was not opened");
  return NULL;
}

static PyObject* PyRecordAt(PyObject *self, PyObject * args){
  if(FOMPython::s_InReader==0){
    PyErr_SetString(PyExc_RuntimeError, "Streaming file was not opened");
    return NULL;
  }
  size_t pos=0;
  if(!PyArg_ParseTuple(args,"K",&pos)){
    return NULL;
  }
  if(pos>=FOMPython::s_InReader->size()){
    PyErr_SetString(PyExc_RuntimeError, "Trying to access non-existent record");
    return NULL;
  }
  auto r = FOMPython::s_InReader->at(pos);
  if(!r.getHeader()){
    Py_INCREF(Py_None);
    return Py_None;  
  }
  int count=0;
  auto stackArray=r.getStacks(&count);
  PyObject* stackList=PyTuple_New(count);
  for(int i=0;i<count;i++){
    PyTuple_SetItem(stackList,i,PyInt_FromLong(stackArray[i]));
  }
  PyObject* resultList=PyList_New(7);
  PyList_SetItem(resultList,0,Py_BuildValue("K",r.getTStart()));
  PyList_SetItem(resultList,1,Py_BuildValue("K",r.getTReturn()));
  PyList_SetItem(resultList,2,Py_BuildValue("K",r.getTEnd()));
  PyList_SetItem(resultList,3,Py_BuildValue("B",r.getAllocType()));
  PyList_SetItem(resultList,4,Py_BuildValue("K",r.getAddr()));
  PyList_SetItem(resultList,5,Py_BuildValue("K",r.getSize()));
  PyList_SetItem(resultList,6,stackList);
  return resultList;
  
}


static PyMethodDef methods[]= 	{{(char *)"mergePages", (PyCFunction)PyMergePages, METH_VARARGS, NULL},
				 {(char *)"parseMergedOutputFiles", (PyCFunction)PyParseMergedOutputFiles, METH_VARARGS, NULL},
				 {(char *)"parseOutputFiles", (PyCFunction)PyParseOutputFiles, METH_VARARGS, NULL},
				 {(char *)"openMallocFile", (PyCFunction)PyOpenMallocFile, METH_VARARGS, NULL},
				 {(char *)"closeMallocFile", (PyCFunction)PyCloseMallocFile, METH_VARARGS, NULL},
				 {(char *)"getAllocationTraces", (PyCFunction)PyGetAllocationTraces, METH_VARARGS, NULL},
				 {(char *)"address2line", (PyCFunction)PyAddress2Line, METH_VARARGS, NULL},
				 {(char *)"getObjectValidity", (PyCFunction)PyGetObjectValidity, METH_VARARGS, NULL},
				 {(char *)"openStream", (PyCFunction)PyOpenFileAsStream, METH_VARARGS, NULL},
				 {(char *)"closeStream", (PyCFunction)PyCloseFileAsStream, METH_VARARGS, NULL},
				 {(char *)"next", (PyCFunction)PyGetNextFromSteam, METH_VARARGS, NULL},
				 {(char *)"size", (PyCFunction)PyNumRecords, METH_VARARGS, NULL},
				 {(char *)"at", (PyCFunction)PyRecordAt, METH_VARARGS, NULL},
				 { NULL, NULL, 0, NULL }};

PyMODINIT_FUNC initFOMTools(void) { (void) Py_InitModule("FOMTools", methods);  import_array();}

