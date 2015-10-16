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

# Simple script to evaluate the trahshing value for a given application and
# a given memory range

import numpy
import argparse
import time
import os
import sys
import shlex
from subprocess import Popen, call, PIPE

# Parse command line
parser = argparse.ArgumentParser()
parser.add_argument("-c", "--cgroup", dest="cgroup",
                  help="Name of cgroup (memory/freezer) the process shall belong to", metavar="CGROUP")
parser.add_argument("-max", "--max", dest="max",
                  help="Maximum Memory limit", metavar="MAXMEMORYLIMIT")
parser.add_argument("-min", "--min", dest="min",
                  help="Minimum Memory limit", metavar="MINMEMORYLIMIT")
parser.add_argument("-steps", "--steps", dest="steps",
                  help="How Many Memory Steps", metavar="STEPS")
parser.add_argument("-b", "--binary", dest="binary",
                  help="Binary to be executed", metavar="BIN")
parser.add_argument("-d", "--directory", dest="directory",
                  help="Directory to store output data", metavar="DIR")

args = parser.parse_args()
if None in [args.cgroup, args.min, args.max, args.binary]:
  parser.error("Incorrect number of arguments. Exit...")
  sys.exit()
if args.steps == None:
  args.steps = 10

# Interpret Min, Max
if 'K' in args.min:
   min = int(args.min.split('K')[0])*1024
if 'K' in args.max:
   max = int(args.max.split('K')[0])*1024
if 'M' in args.min:
   min = int(args.min.split('M')[0])*1024*1024
if 'M' in args.max:
   max = int(args.max.split('M')[0])*1024*1024
if 'G' in args.min:
   min = int(args.min.split('G')[0])*1024*1024*1024
if 'G' in args.max:
   max = int(args.max.split('G')[0])*1024*1024*1024

# Setup a cgroup subgroup 
try:
  os.mkdir("/cgroup/memory/"+args.cgroup+"/FOM")
except:
  print "\nMemory Cgroup FOM already exist."

environment = dict(os.environ)
cmd = shlex.split(args.binary)

for i in range(min, max, (max-min)/int(args.steps)):

  # Set memory limit
  cmd = 'echo ' + str(i) + ' > /cgroup/memory/' + args.cgroup + '/FOM/memory.limit_in_bytes'
  call(cmd, shell=True)

  # Start process
  try:
     file1 = open("output%s"%i, "w")
     file2 = open("vmstat%s"%i, "w")
     cmd = shlex.split(args.binary)
     p1 = Popen(cmd, env=environment,  stdout=file1, stderr=file1)
     cmd = 'echo ' + str(p1.pid) + ' > /cgroup/memory/' + args.cgroup + '/FOM/tasks'
     call(cmd, shell=True)
     print "\nStart '", args.binary, "' with a memory limit of", i, "bytes - PID:", p1.pid
     p2 = Popen(["vmstat", "1"], stdout=file2 )
  except:
    print "\nA problem occured while starting ", args.binary , " - Exit..." 
    sys.exit()

  while (p1.poll()==None):  
    time.sleep(10)

  p2.terminate()
  
  # Compute thrashing value
  cmd = "cat vmstat%s | awk {'print $3, $7, $8'} | grep -v swap | grep -v si > vmstat%sNew" %(i,i)
  call(cmd, shell=True)
  vmstat = numpy.loadtxt("vmstat%sNew"%i)
  v = numpy.ones(len(vmstat[:,0]))*vmstat[0,0]
  swap = vmstat[:,0] - v
  if numpy.sum(swap) > 0:
    print "Thrashing value ", (numpy.trapz(numpy.cumsum(vmstat[:,2]) - numpy.cumsum(vmstat[:,1]))) / numpy.trapz(swap)
  else:
    print "Process did not swap with the given memory limit"

  

    


   
