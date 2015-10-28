#!/usr/bin/env python

# Copyright (c) CERN 2015
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

# Generate statistics about continous blocks of pages in RAM/Swap/not loaded
# and plot histograms 

import FOMTools 
from subprocess import Popen, call, PIPE
import os
import numpy
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from scipy.stats import itemfreq
import json

if os.path.isfile("FOMSummary.json"):
  f = open('FOMSummary.json')
  dict = json.loads(f.read())
  mfiles = dict["Output Files"]


def plotHistograms(i, status):
   matrix       = FOMTools.parseMergedOutputFiles(mfiles[i]+".merged.sorted")
   tmp     = matrix.shape[0]/4
   matrix.shape = (tmp,4)
   m     = matrix[numpy.where(matrix[:,3] == status)[0]]

   if m.shape[0] > 1:
    a = itemfreq(m[:,1]) 
    ax.clear()
    ax.set_title(mfiles[i] + " - " + str(m[:,1].sum()) + " out of " + str(matrix[:,1].sum()) + " pages" )
    axes.set_xscale('log')
    axes.set_yscale('log')
    ax.autoscale()
    bars = ax.bar(a[:,0],a[:,1],1)
    plt.xlabel("Number of continous pages")
    plt.ylabel("Number of occurences") 
    plt.draw()

# Get Histograms for blocks of continous pages in RAM
fig = plt.figure()
ax = fig.add_subplot(1,1,1)
axes = plt.axes()
fig.suptitle("Statistics for blocks of pages in RAM")
ani = animation.FuncAnimation(fig, plotHistograms, len(mfiles), fargs=(10,), repeat=False)
plt.show()

# Get Histograms for blocks of continous pages in Swap
fig = plt.figure()
ax = fig.add_subplot(1,1,1)
axes = plt.axes()
fig.suptitle("Statistics for blocks of pages in Swap")
ani = animation.FuncAnimation(fig, plotHistograms, len(mfiles), fargs=(1,), repeat=False )
plt.show()

# Get Histograms for blocks of continous pages never loaded 
fig = plt.figure()
ax = fig.add_subplot(1,1,1)
axes = plt.axes()
fig.suptitle("Statistics for blocks of pages never loaded")
ani = animation.FuncAnimation(fig, plotHistograms, len(mfiles), fargs=(0,), repeat=False )
plt.show()