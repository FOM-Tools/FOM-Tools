#!/usr/bin/env python

 #
 #  Copyright (c) CERN 2015
 #
 #  Authors:
 #      Nathalie Rauschmayr <nathalie.rauschmayr_ at _ cern _dot_ ch>
 #      Sami Kama <sami.kama_ at _ cern _dot_ ch>
 #
 #  Licensed under the Apache License, Version 2.0 (the "License");
 #  you may not use this file except in compliance with the License.
 #  You may obtain a copy of the License at
 #
 #      http://www.apache.org/licenses/LICENSE-2.0
 #
 #  Unless required by applicable law or agreed to in writing, software
 #  distributed under the License is distributed on an "AS IS" BASIS,
 #  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 #  See the License for the specific language governing permissions and
 #  limitations under the License.
 #

import os
import sys
import FOMTools 
import json
from subprocess import Popen, call, PIPE
import numpy

def printValues(m, n):
  for i in range(1,n+1):
   if i < m.shape[0]:
     print "    ", i, hex(m[-i,0]),"\t",int(m[-i,1]),"\t",hex(m[-i,2])

# Load some parameters needed for postprocessing 
if os.path.isfile("FOMSummary.json"):
  f = open('FOMSummary.json')
  dict = json.loads(f.read())
  count = dict["Number Of Output Files"]
  max = dict["Maximum Heapsize"]
  ranges = dict["Heap Sizes"]
  files = dict["Output Files"]
  pid = str(dict["Pid"])
  mallocFile = dict["Malloc Output File"].rstrip("%p")
else:
 print "   Cannot find FOMSummary.json"
 sys.exit() 
 
# Prepare files for postprocessing
print "Prepare files for postprocessing:"
print "   Merge output files"
# Merge and Sort Output Files
for i in files:
  FOMTools.mergePages(i, i)

#Translate function+offsets into source lines
print "   Translate function+offsets into source lines"
if "%p" in mallocFile:
  mallocFile = mallocFile.replace("%p", pid)
if  not os.path.isfile(mallocFile+"_sourcelines"):
  FOMTools.address2line(mallocFile+"_symbolLookupTable", mallocFile+"_maps", mallocFile+"_sourcelines")

c = Popen("ls iteration*.merged.sorted", shell=True, stdout=PIPE, close_fds=True)
mfiles = c.stdout.read().split()

#Start posptrocessing
print "\nStep 1: Find largest blocks of continous pages in RAM/Swap and not loaded"
# Top 10 blocks in RAM, Swap, Not Loaded
count = 0
for i in mfiles:
   count = count + 1
   matrix   = FOMTools.parseMergedOutputFiles(i)
   tmp     = matrix.shape[0]/4  
   matrix.shape = (tmp,4)
   ram     = matrix[numpy.where(matrix[:,3] == 10)[0]]
   swap    = matrix[numpy.where(matrix[:,3] == 1)[0]]
   nloaded = matrix[numpy.where(matrix[:,3] == 0)[0]]

   print "   Top 10 in iteration", count 
   print "   In RAM: \n", 
   printValues(ram, 10)
   print "   In Swap: \n", 
   printValues(swap, 10)
   print "   Not Loaded: \n", 
   printValues(nloaded, 10)
   print "\n"

# Get stacktraces for top 10
print "Step 2: Find objects that belong to the range of the 10 largest blocks of swapped pages\n"
#if False:
if (os.path.isfile(str(mallocFile)) and os.path.isfile(str(mallocFile)+"_sourcelines")):
  FOMTools.openMallocFile(str(mallocFile))
  for i in range(1,11):
    print "Hotspot", i, ": ", 
    FOMTools.getAllocationTraces(swap[-i,0], swap[-i,2], "Hotspot%s"%i, str(mallocFile)+"_sourcelines", 0)
  FOMTools.closeMallocFile()
  print "Objects written to outputfiles 'Hotspot[0-9]'\n"

#Cleanup
del ram
del swap
del nloaded
del matrix

print "Step 3: Find number of pages always in RAM and most of the time in Swap\n"
# Determine iterations without swapping
nIterationsNoSwapping = 0
newFiles = []
newRanges = []

for i in range(0,len(files)):
  matrix  = FOMTools.parseOutputFiles(files[i])
  tmp     = matrix.shape[0]/2
  matrix.shape = (tmp,2)
  if len(matrix[numpy.where(matrix[:,1] == 1 )]) == 0:
    nIterationsNoSwapping = nIterationsNoSwapping + 1 
  else:
    # only consider files where  swapping occured
    newFiles.append(files[i])
    newRanges.append(ranges[i])
  del matrix
print "  ", nIterationsNoSwapping, "Iterations during which the  memory limit was not reached"
del files
del mfiles
del ranges

# Compute number of pages always in RAM/Swap
alwaysInRam  = 0
alwaysInSwap = 0
begin = 0
a = numpy.arange(1, len(newRanges)+1)
indexes = numpy.argsort(newRanges)
for l in range(0, len(newRanges)):

  end = int(newRanges[indexes[l]])
  if end > begin:

    tmp = end - begin
    ram  = numpy.zeros((count,tmp))
    swap = numpy.zeros((count,tmp))

    for i in range(0, len(newFiles)):
      matrix = FOMTools.parseOutputFiles(newFiles[i], begin, end)
      if len(matrix) > 0: 
        tmp     = matrix.shape[0]/2
        matrix.shape = (tmp,2)
        tmpRam  = numpy.array(matrix[:,1], copy=True)
        ind     = (tmpRam == 1 ) # ignore pages in Swap
        tmpRam[ind] = 0
        ram[i,0:tmp]  = tmpRam
      
        tmpSwap = numpy.array(matrix[:,1], copy=True)
        ind     = (tmpSwap == 10 ) # ignore pages in RAM
        tmpSwap[ind] = 0
        swap[i,0:tmp]  = tmpSwap * (i+1)# for weighted sum
      del matrix
    sumRam  = ram.sum(axis=0)
    sumSwap = swap.sum(axis=0)
    del tmpRam
    del tmpSwap

    begin = end
    alwaysInRam  = alwaysInRam + len(sumRam[numpy.where(sumRam == (len(newRanges)-l)*10)])
    # If limit is not low enough pages will always be in RAM first, therefore apply a lower threshold 
    threshold    = numpy.sum(a[l:len(newRanges)]) 
    alwaysInSwap = alwaysInSwap + len(sumSwap[numpy.where(sumSwap >= 0.95 * threshold)])

print "   Number of Pages always in RAM:", alwaysInRam
print "   Number of Pages most of the time in Swap:", alwaysInSwap 
