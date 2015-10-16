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

# This script analyses all recorded stacktraces, detects allocations
# involving ROOT, make some statistics and dumps dictionaries to an output file

#!/bin/env python
import sys
import os
import numpy
import operator
import json
from subprocess import Popen, PIPE

# get stack IDs and corresponding ROOT libraries
def getStackLibMaps(filename):

  f = open(filename + "_sourcelines")
  stack2lib = {} # dictionary: key = stackID and value = corresponding ROOT library
  libCount  = {} # dictionary: key = ROOT library and value = another dictionary whose keys are stackIDs and values are the  number of occurences of the corresponding stackID 
  lib2stack = {} # dictionary: key = ROOT library and values = stackIDs

  for i in f.readlines():
     i = i.strip()
     tmp = i.split()
     if "/ROOT" not in tmp[1]:
         continue
     stack2lib[tmp[0]] = tmp[1]
     if tmp[1] not in libCount:
         libCount[tmp[1]]=[0,0,{},{},{},{}] # lib call count, lib total mem, calls per stack id, [ first allocation, counts of allocation ] per address, {initiating stack id: counts } per stack id, {initiating stack id on non-broken root chain:counts} 
         lib2stack[tmp[1]]=[tmp[0]]
     if tmp[0] not in lib2stack[tmp[1]]:
         lib2stack[tmp[1]].append(tmp[0])

  for l,s in lib2stack.iteritems():
    for st in s:
      libCount[l][2][st]=0

  # Remove TObject::new and TStorage::Allocator
  cmd = """ grep -a -E \(TObjectnw\|TStorage\) """ + filename + """_symbolLookupTable  |sed -e "s/\\t.*//" """
  c = Popen(cmd, shell=True, stdout=PIPE, close_fds=True)
  ignoredStackIds = c.stdout.read().split()
  for i in ignoredStackIds:
    stack2lib.pop(i,None)

  return [stack2lib,libCount,lib2stack]

# Scan all stacktraces and find initiating and terminating ROOT libraries
def scan(stack2lib,libCount,lib2stack, filename, verbose):

  f = open(filename + ".txt")
  count = 0
  for line in f:
    count += 1
    if (count % 1000000) == 0:
      print "Analyzed", count, "lines in ", filename + ".txt"
      sys.stdout.flush()
#    if count == 100000:
#      break
    line = line.strip()
    tmp = line.split(":")
    tmp2 = tmp[1].split()
    iter = 0
    for i,l in enumerate(tmp2[:20]): # scan stack traces. Only consider the first 20 in order to neglect function calls necessary for the Python/C++ interface that involves PyROOT but does actually not do allocations
      iter = iter + 1
      if l in stack2lib: # if stack id is in one of root libs
        ids = tmp[0].split()
        size = int(ids[5])
        ltup = libCount[stack2lib[l]]
        ltup[0]  += 1 #increment lib count
        ltup[1]  += size # increment lib mem size
        ltup[2][l] += 1 #increment stack 
        addr = int(ids[4],16)
        if addr in ltup[3] : # if address is seen before
            ltup[3][addr][1] += 1 # increment same address allocation count
        else:
          ltup[3][addr] = [tmp[0],1] # create same address 
        for isid in reversed(tmp2):
          if isid in stack2lib: #if isid is a root stack
            hist2d = ltup[4] #reference to 2d histos
            if not l in hist2d: # if histogram doesn't contain the id
              hist2d[l] = {isid:[1,size]} #create a dictionary for initiators
            else:
              if isid not in hist2d[l]:
                hist2d[l][isid] = [0,0]
              hist2d[l][isid][0] += 1 # increment initiator count
              hist2d[l][isid][1] += size #increment initiator size
            break
        oldStack = l
        for isid in tmp2[i:]:
          if isid not in stack2lib: #if isid is a root stack
            hist2d = ltup[5] #reference to 2d histos
            if not l in hist2d: # if histogram doesn't contain the terminator id
              hist2d[l] = {oldStack:[1,size]} #create a dictionary for initiators
            else:
              if oldStack not in hist2d[l]:
                hist2d[l][oldStack] = [0,0]
              hist2d[l][oldStack][0] += 1 # increment initiator count
              hist2d[l][oldStack][1] += size #increment initiator size
            break
          oldStack=isid
        break

  # dump to file
  print "Scanning finished. Dumping dictionary to tuple.json "
  f = open("tuple.json",'w')
  json.dump([stack2lib,libCount],f,indent=4,sort_keys=True)

  # print allocations to seperate file
  f = open("Allocated_ROOT_Objects","w")
  for k,v in libCount.iteritems():
    tmpList = [[add,val[0].split(),val[1]] for add,val in v[3].iteritems() if val[1] == 1 ]
    for vv  in sorted(tmpList,key=lambda l: l[0] ):
      if int(vv[1][5]) > 4095: # consider onle allocations larger than one page
         f.write("%f %d %d %d %d %d\n" %(float(vv[1][0]), int(vv[1][1]), int(vv[1][2],16), int(vv[1][3],16), int(vv[1][4],16), int(vv[1][5])))

  # print some statistics if verbose
  if verbose:
    liblist=[[k,v[0],v[1],v[2]] for k,v in libCount.iteritems()]
    count_sorted = sorted(liblist,key=lambda lib: lib[1])

    print "\nCall count sorted\n"
    for sl in count_sorted:
      print sl[1],sl[2],sl[0]
    size_sorted = sorted(liblist,key=lambda lib: lib[2])

    print "\nSize sorted\n"
    for sl in size_sorted:
      print sl[2],sl[1],sl[0]

    print "\nCall counts\n"
    for sl in count_sorted:
      print sl[0],sl[3]

    print "\nCall correlation counts\n"
    for k,v in libCount.iteritems():
      print k
      hist2d = v[4]
      for term,d in hist2d.iteritems():
        h = {}
        for init, count in d.iteritems():
          h[init] = count[0]
        print term,h

    print "\nCall correlation Sizes\n"
    for k,v in libCount.iteritems():
      print k
      hist2d = v[4]
      for term,d in hist2d.iteritems():
        h = {}
        for init, count in d.iteritems():
          h[init] = count[1]
        print term,h

# Load some parameters 
if os.path.isfile("FOMSummary.json"):
  f = open('FOMSummary.json')
  dict = json.loads(f.read())
  pid = str(dict["Pid"])
  mallocFile = dict["Malloc Output File"]
  if "%p" in mallocFile:
    mallocFile = mallocFile.replace("%p", pid)
else:
 print "Cannot find FOMSummary.json"
 sys.exit()
      
if not os.path.isfile(mallocFile+'.txt'):
  print "Could not find", mallocFile, ".txt . Execute first: ./binRecord2txt", mallocFile
  sys.exit()

if not os.path.isfile(mallocFile+'_sourcelines'): 
  import FOMTools
  FOMTools.address2line(mallocFile+"_symbolLookupTable", mallocFile+"_maps", mallocFile+"_sourcelines")

# Generate dictionary to map libs to stackIDs and vice versa
d = getStackLibMaps(mallocFile)
print "Total number of different function calls to Root:", len(d[0]), "Root libs: ", len(d[1])
print "Number of stackIDs in each Root library"
nlibstacks = sorted([[k,v] for k,v in d[2].iteritems()], key=lambda l:len(l[1]))
for i in nlibstacks:
  print i[0],len(i[1])

# Generate 2D histogram and dump it to a file
scan(d[0],d[1],d[2], mallocFile, verbose=False)

