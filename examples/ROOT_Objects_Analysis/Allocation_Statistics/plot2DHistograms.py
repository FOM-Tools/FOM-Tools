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

# This script reads the dictionary from tuple.json that contains initiating and
# terminating function calls for malloc/realloc/calloc involving ROOT libraries.
# It creates then a 2d histogram for each ROOT library for total allocated size and
# number of malloc calls. The data will be filtered, such that combinations < 1% are 
# removed. It plots then the remained data normed to the total number of malloc calls/
# total number of alloacted memory involving ROOT

#!/bin/env python

import numpy as np
import matplotlib.pyplot as plt
import json
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import pyplot,cm
import matplotlib as mpl
import matplotlib.colors as colors   
import numpy

# define a new colormap 
oldCmap = plt.get_cmap('hot')
minval=0
maxval=0.7
newCmap = colors.LinearSegmentedColormap.from_list( 'trunc({n},{a:.2f},{b:.2f})'.format(n=oldCmap.name, a=minval, b=maxval), oldCmap(np.linspace(minval, maxval, 100)))

def plotMap(name,histMap,lenX,title):

    cutVal=0.01 # filter
    normValue=0.
    for term in histMap.values():    #integrate over lib
      for c in term.values():
          normValue+=c
    print "normValue=",normValue
    yLabels=[]
    xLabels=[]

    for term,v in histMap.iteritems(): #loop over all
      for init,c in v.iteritems():
         cold=c
         c = float(c)/normValue
         histMap[term][init]=c #normalize to 1
         if c>cutVal : #if above threshod add labels to image
           if term not in yLabels:
             yLabels.append(term)
           if init not in xLabels:
             xLabels.append(init)

    if len(yLabels) == 0 or len(xLabels) == 0:
      print "Lib does not have anything above cut value", cutVal, name
      return

    # define ylabels and xlabels
    yLabels=sorted(yLabels)
    xLabels=sorted(xLabels)
    pos=0
    yMap={}
    xMap={}
    for l in yLabels:
      yMap[l]=pos
      pos+=1
    pos=0
    for l in xLabels:
      xMap[l]=pos
      pos+=1

    lenY=len(yLabels)
    lenX=len(xLabels)
    counts=np.zeros((lenX,lenY))
    print len(xLabels),len(yLabels)
    
    # create image matrix
    for term,v in histMap.iteritems(): 
      if term in yMap:
        kpos=yMap[term]
        for init,c in v.iteritems():
          if c>cutVal:
            print "term =",term,"init=", init, c
            counts[xMap[init],kpos]=float(c)
    del xMap,yMap

    counts=counts.T #imshow displays transpose
    if lenX<5 and lenY < 5:
      print counts

    # plot the image
    fig=plt.figure()
    ax=fig.add_subplot(111)
    xbins=np.arange(lenX)
    ybins=np.arange(lenY)
    plt.xticks(xbins+0.5,xLabels,rotation=45)
    plt.yticks(ybins+0.5,list(reversed(yLabels)),rotation=45)
    plt.xlabel("Initiator",  fontsize=15)
    plt.ylabel("Terminator",  fontsize=15)
    plt.rc('font', size=12)
    plt.title("%s (above %s%% of total)"%(title,cutVal*100)) 
    masked=numpy.ma.masked_where(counts == 0, counts)
    plt.imshow(masked,cmap=newCmap,interpolation='nearest',aspect='auto',extent=[0,lenX,0,lenY],alpha=1)
    cbar=plt.colorbar()
    plt.tight_layout() 
    plt.imsave(name,masked,cmap=newCmap)
    plt.savefig(name)
    plt.close()

    # cleanup
    del fig
    del ax

    return

def plotMaps(libMaps,suffix):
    count=0
    for k,v in libMaps.iteritems():
        count+=1
        n=k.split("/")[-1][:-3]
        print "plotmaps k",k,v[1],n
        if len(v[0]) > 0:
          plotMap(n+suffix+".png",v[0],v[1],"Different function calls terminating in %s"%(n))

f=open("tuple.json")
stackIds,maps=json.load(f)

# Get the first and last root library for each stacktrace (broken chain)
# Obtain number of malloc calls and  build 2 histogram
callHists={}
for k,v in maps.iteritems():
    callHists[k]=[{},0]
    calls=callHists[k][0]
    hist2d=v[4] # broken chain
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        calls[term]=h
        for init,count in d.iteritems():
            h[init]=count[0]  # number of malloc calls
        if len(h) > lenX: lenX=len(h)
    callHists[k][1]=lenX

# Get the first and last root library for each stacktrace (broken chain)
# Obtain the total size if alloacted memory and  build 2 histogram
sizeHists={}
for k,v in maps.iteritems():
    sizeHists[k]=[{},0]
    sizes=sizeHists[k][0]
    hist2d=v[4] # broken chain
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        sizes[term]=h
        for init,count in d.iteritems():
            h[init]=count[1] # size of allocated memory
        if len(h) > lenX: lenX=len(h)
    sizeHists[k][1]=lenX

# Plot the 2d histograms
plotMaps(callHists,"Calls")
plotMaps(sizeHists,"MallocSize")

# Get the first and last root library for each stacktrace (unbroken chain)
# Obtain number of malloc calls and  build 2 histogram
callHists={}
for k,v in maps.iteritems():
    callHists[k]=[{},0]
    calls=callHists[k][0]
    hist2d=v[5] # unbroken chain
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        calls[term]=h
        for init,count in d.iteritems():
            h[init]=count[0] # number of malloc calls
        if len(h) > lenX: lenX=len(h)
    callHists[k][1]=lenX

# Get the first and last root library for each stacktrace (unbroken chain)
# Obtain the total size if alloacted memory and  build 2 histogram
sizeHists={}
for k,v in maps.iteritems():
    sizeHists[k]=[{},0]
    sizes=sizeHists[k][0]
    hist2d=v[5] # unbroken chain
    lenX=0
    for term,d in hist2d.iteritems():
        h={}
        sizes[term]=h
        for init,count in d.iteritems():
            h[init]=count[1] # size of allocated memory
        if len(h) > lenX: lenX=len(h)
    sizeHists[k][1]=lenX

# Plot the 2d histograms
plotMaps(callHists,"UnbrokenChainCalls")
plotMaps(sizeHists,"UnbrokenChainMallocSize")
