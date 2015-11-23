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

#!/bin/env python
import FOMTools
import os
import json
import numpy
import sys
import resource 
import subprocess

if os.path.isfile("FOMSummary.json"):
  f = open('FOMSummary.json')
  dict = json.loads(f.read())
  max = dict["Maximum Heapsize"]
  files = dict["Output Files"]
  ranges = dict["Heap Sizes"]
  max = dict["Maximum Heapsize"]
  pid = str(dict["Pid"])

if not os.path.isfile("All_ROOT_Objects.txt"):
  if not os.path.isfile("Allocated_ROOT_Objects"):
    print "Could not find the file 'Allocated_ROOT_Objects'. Please run first: python ./examples/ROOT_Objects_Analysis/Persistent_Objects/generateHistograms.py"
    sys.exit()
 
  matrix = numpy.loadtxt("Allocated_ROOT_Objects")
 
  n = matrix.shape[0]
  # needs to be preallocated beforehand to make sure it is contigous
  beginTmp = numpy.zeros((n), dtype=numpy.uint64)
  timeTmp  = numpy.zeros((n), dtype=numpy.double)
  endTmp   = numpy.zeros((n), dtype=numpy.uint64)
  adrTmp   = numpy.zeros((n), dtype=numpy.uint64)

  # make sure array remains contigous
  for i in range(0,n): 
    beginTmp[i] = matrix[i,2]
    endTmp[i]   = matrix[i,3]
    timeTmp[i]  = matrix[i,0]
    adrTmp[i]   = matrix[i,4]

  del matrix 

  # get the lifetimes for all ROOT objects 
  FOMTools.openMallocFile(str("mallocOutput_"+ str(pid) + ".fom"))
  lifetimes = FOMTools.getObjectValidity(beginTmp, endTmp, timeTmp, adrTmp)
  FOMTools.closeMallocFile()
  print lifetimes 
  # Filter out shortlived ROOT objects: -1 means that the 
  # object lived until the process finished
  indexes = numpy.where(lifetimes == -1)[0]
  time    = numpy.array(timeTmp[indexes], copy=True)
  begin   = numpy.array(beginTmp[indexes], copy=True)
  end     = numpy.array(endTmp[indexes], copy=True)

  print "Shortlived Objects:", len(numpy.where(lifetimes != -1)[0])

  dim = timeTmp.shape[0]
  timeTmp.shape = (dim,1)
  beginTmp.shape= (dim,1)
  endTmp.shape	= (dim,1)
  adrTmp.shape	= (dim,1)
  lifetimes.shape= (dim,1)

  # save to file
  matrix = numpy.hstack([timeTmp,beginTmp,endTmp,adrTmp,lifetimes])
  numpy.savetxt("All_ROOT_Objects.txt", matrix, fmt="%f %x %x %x %d")

  # cleanup
  del beginTmp
  del endTmp
  del timeTmp
  del adrTmp

  dim = time.shape[0]
  m = numpy.hstack([time,begin,end])
  m.shape = (3,dim)
  numpy.savetxt("Persistent_ROOT_Objects.txt",m.T , fmt="%f %d %d")

else:
  m     = numpy.loadtxt("Persistent_ROOT_Objects.txt")
  time  = m[:,0]
  begin = m[:,1]
  end   = m[:,2]

# Sort persistent ROOT objects by time
sortIndexes = numpy.argsort(time)
time  = time[sortIndexes]
begin = begin[sortIndexes]
end   = end[sortIndexes]

# Find the iteration with the maximum heapsize
# The corresponding output file serves as "base" file 
for i in range(0,len(ranges)):
  if ranges[i] == max:
    bFile = files[i]
    break

# Get status for each page of the "base" file
matrix  = FOMTools.parseOutputFiles(bFile)
tmp     = matrix.shape[0]/2
matrix.shape = (tmp,2)
ind = []

# Get indexes that match a persistent ROOT object. This step is done in order 
# to the lookup the indexes only once rather than for each iteration file. 
# Once we have the indices it needs only to be verified in each iteration, 
# whether  timestamp and indexes match. 
for i in range(0, len(begin)):
  indexes = numpy.where((matrix[:,0] >= begin[i]) & (matrix[:,0] <= end[i])) 
  ind.append([indexes[0]])

# iterate over each iteration output file
for i in range(0,len(files)):
  # read iteration output file
  matrix  = FOMTools.parseOutputFiles(files[i])
  tmp     = matrix.shape[0]/2
  matrix.shape = (tmp,2)

  # buffer init, all entries are set to -1
  buf = numpy.ones((tmp,3))
  buf = buf * (-1)
  buf[:,0] = buf[:,0]*(-(i+1))
  buf[:,1] = matrix[:,0]

  # get the timestamp of the iteration
  f = open(files[i], "r")
  timestamp = float(f.readline())
  f.close()
  index2 = len(numpy.where(time <= timestamp)[0]) #number of ROOT allocations made before or within the timestamp

  # if ROOT objects were found that were allocated in this iteration or before 
  if index2 > 0:
   index3 = ind[0:index2] #get matrix ids belonging to the allocations that match timestamp
   index4 = numpy.hstack(index3) #transform numpy arrays to one big numpy array
   
   # the heap can shrink if objects are freed. In this case some IDs need to be filtered out
   if not numpy.max(index4) < tmp:
     indTmp = numpy.array([])
     for b in index4[0]:
       if b < tmp:
         indTmp = numpy.append(indTmp, b)
     index4 = indTmp   

   # index4 contains all ids for pages containing a persitent ROOT object
   # Read page status of these pages from matrix and set the values to buffer  
   ind5 = (index4,)
   buf[ind5,2] = matrix[ind5,1]  
  
  # Print a new output file. Each page not containing any ROOT object is set to -1 
  print "Writing ", files[i] + "_ROOT" 
  numpy.savetxt(files[i]+"_ROOT", buf, fmt='%d') 

# get resolution png plot
with open(files[1], "r") as f:
  time = next(f).decode()
  first = next(f).decode()
  xmin = int(first.split()[2])

xmax = max*resource.getpagesize() + xmin
y = len(files)

# Generate Gnuplot script with correct resolution
cmd = """set terminal png size %d,%d font "/usr/share/fonts/dejavu/DejaVuSansMono.ttf" 
#set palette defined (1 "red",0 "black", 10 "green")
set output "output.png"
unset key
unset colorbox
set lmargin 0
set rmargin 0
set tmargin 0
set bmargin 0
set yrange [0:%d]
set xrange [%d.0:%d.0]
unset tics
unset border
plot """%((xmax-xmin)/4096.0 , y , y+1, xmin-1, xmax+1)

for i in range(1,y):
  cmd += "'iteration%04d_ROOT'"%i + " using 2:1:3  w dots lc variable,"
cmd += " 'iteration%04d_ROOT'"%(i+1) + " using 2:1:3  w dots lc variable\n"

# Write file
f = open("cmd.txt", "w")
f.write(cmd)
f.close()

# Execute Gnuplot and perform cleanup
subprocess.call(["gnuplot", "cmd.txt"])