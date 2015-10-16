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
import numpy
import matplotlib.pyplot as plt
import matplotlib.colors as colors
import json

# This script reads the dictionary from tuple.json that contains initiating and
# terminating function calls for malloc/realloc/calloc involving ROOT libraries. 
# The script merges function calls by library and plots the percentage of total 
# allocated memory sizes and calls as a 2D histogram 

def getData(index):

  f=open("tuple.json")
  stack2lib,libCount=json.load(f)

  x = numpy.array([])
  y = numpy.array([])
  z = numpy.array([])
  libIds={}
  libId=0
  terminatorIds={}
  initiatorIds={}
  terminatorH=0
  initiatorH=0
  img={}
  terminatorLib =None 

  # Iterate over dictionary and merge stackIDs by libraries
  for k,v in libCount.iteritems():
    hist2d=v[5]
    h={}
    lib=k.split('/')[-1]
    lib=lib.split('.')[0]+".so" # obtain library name
    if lib not in libIds:
      libIds[lib]=libId
      libId+=1

    for term,d in hist2d.iteritems():
      terminatorLib = stack2lib[term].split('/')[-1]
      terminatorLib = terminatorLib.split('.')[0]+".so"
      if terminatorLib not in terminatorIds:
        terminatorIds[terminatorLib] = terminatorH
        terminatorH += 1

      for init, count in d.iteritems():
        initiatingLib = stack2lib[init].split('/')[-1]
        initiatingLib = initiatingLib.split('.')[0]+".so"
        if initiatingLib not in initiatorIds:
          initiatorIds[initiatingLib]=initiatorH
          initiatorH+=1
        if initiatorIds[initiatingLib] not in h:
          h[initiatorIds[initiatingLib]]=count[index]
        else:
          h[initiatorIds[initiatingLib]]+=count[index]              

    if terminatorLib != None:
     tmp = numpy.ones(len(h.keys()))*int(terminatorIds[terminatorLib])
    else:
     tmp = [] 
    x = numpy.append(x, tmp)# terminator indices
    y = numpy.append(y, map(int, h.keys()))# initiator
    z = numpy.append(z, h.values())
    img[lib]=h

  # Create z matrix and fill it
  zMatrix = numpy.zeros((initiatorH,len(terminatorIds.keys())))

  for k,v in img.iteritems():
    if k not in terminatorIds : continue
    termPos=terminatorIds[k]
    for ini,count in v.iteritems():
        zMatrix[ini,termPos] = count/numpy.sum(z)
        for t in initiatorIds.keys():
          if initiatorIds[t] == ini:
            print k, t, count#/numpy.sum(z)

  # Create x and y labels. Y labels must be reversed because of imshow()
  ylabels=list(reversed(sorted(terminatorIds.items(), key=lambda x:x[1])))
  xlabels=sorted(initiatorIds.items(), key=lambda x:x[1])
  xlabelsNew = []
  ylabelsNew = []
  for i in xlabels:
    for l in initiatorIds.keys():
      if l in i and l not in xlabelsNew:    
        xlabelsNew.append(l)
  for i in ylabels:
    for l in terminatorIds.keys():
      if l in i and l not in ylabelsNew:
        ylabelsNew.append(l)

  x2=numpy.arange(0,len(xlabels))
  y2=numpy.arange(0,len(ylabels))
  maskedArray=numpy.ma.masked_where(zMatrix == 0, zMatrix)

  return x2, y2, maskedArray, xlabelsNew, ylabelsNew


def plotData(x, y, z, xlabels, ylabels, title, name ):

  fig = plt.figure()
  ax = fig.add_subplot(111)
  cmap = plt.get_cmap('hot')
  minval=0
  maxval=0.7
  newCmap = colors.LinearSegmentedColormap.from_list('trunc({n},{a:.2f},{b:.2f})'.format(n=cmap.name, a=minval, b=maxval), cmap(numpy.linspace(minval, maxval, 100)))
  res = ax.imshow(z.T, cmap=newCmap, interpolation='nearest', extent=[numpy.min(x)-0.5,numpy.max(x)+0.5,numpy.min(y)-0.5,numpy.max(y)+0.5], vmin=numpy.min(z), vmax=numpy.max(z))
  plt.title(title)
  plt.xlabel("Initiator",  fontsize=15)
  plt.ylabel("Terminator",  fontsize=15)
  plt.rc('font', size=12)
  fig.colorbar(res)
  plt.xticks(x, xlabels, rotation=90)
  plt.yticks(y, ylabels)
  plt.tight_layout()
  plt.imsave(name,z,cmap=newCmap)
  plt.savefig(name)
  plt.close()

if __name__ == "__main__":

  print "\n Total number of malloc calls in %\n"
  name = "ROOT_Libraries_Malloc_Calls"
  title = "Total number of malloc calls in %"
  t = 0 # number of malloc calls
  x, y, z, xlabels, ylabels = getData(t)
  plotData(x, y, z, xlabels, ylabels, title, name)

  print "\n Total allocated memory in %\n"
  name = "ROOT_Libraries_Allocated_Memory"
  title = "Total allocated memory in %"
  t = 1 # total allocated memory
  x, y, z, xlabels, ylabels = getData(t)
  plotData(x, y, z, xlabels, ylabels, title, name)
