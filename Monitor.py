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

# Sets up the environment, configures cgroup, starts the process and 
# coordinates the monitoring loop 

import os
import argparse 
import sys
from FOMTools import PageStatusChecker 
import time
import shlex
import signal
from subprocess import Popen, call, PIPE
import json
import resource 
import code, traceback

# Signal handler in order to debug the script
def signalHandlerSIGUSR2(signal, frame):
    d={'_frame':frame}         
    d.update(frame.f_globals)  
    d.update(frame.f_locals)

    i = code.InteractiveConsole(d)
    message  = "Signal received"
    message += ''.join(traceback.format_stack(frame))
    i.interact(message)

signal.signal(signal.SIGUSR2, signalHandlerSIGUSR2)

# Signal handler in case script is killed, need to unfreeze process  
def signalHandlerSIGINT(signal, frame):
  print "Received SIGINT. Perform Cleanup. Write Summary."
  cmd = 'echo THAWED > /cgroup/freezer/' + args.cgroup + '/FOM/freezer.state'
  call(cmd, shell=True)
  dict ={"Executed Command": args.binary, "Memory Limit": args.limit, 
         "Interval": args.interval, "Stacktrace Depth":  os.environ["MALLOC_INTERPOSE_DEPTH"], 
         "Interpose Shift": os.environ["MALLOC_INTERPOSE_SHIFT"], 
         "Maximum Heapsize": max,"Number Of Output Files": iteration, 
         "Pid": p.pid, "Malloc Output File": os.environ["MALLOC_INTERPOSE_OUTFILE"], 
         "Heap Sizes": heapsizes, "Output Files": outputFiles }
  with open('FOMSummary.json', 'w') as fp:
    json.dump(dict, fp)
  sys.exit(0)
signal.signal(signal.SIGINT, signalHandlerSIGINT)

pagesize = resource.getpagesize()
# Check where cgroup is located
cgroupPath=""
if os.path.isdir("/sys/fs/cgroup/memory"):
   cgroupPath = "sys/fs/cgroup"
elif os.path.isdir("/cgroup/memory"):
   cgroupPath = "/cgroup"
elif os.path.isdir("/mnt/cgroup/memory"):
   cgroupPath = "/mnt/cgroup"
else:
  print "Can't find cgroup directory."
  sys.exit()

# Parse command line
parser = argparse.ArgumentParser()
parser.add_argument("-c", "--cgroup", dest="cgroup",
                  help="Name of cgroup (memory/freezer) the process shall belong to", metavar="CGROUP")
parser.add_argument("-l", "--limit", dest="limit",
                  help="Memory limit", metavar="MEMORYLIMIT")
parser.add_argument("-i", "--interval", dest="interval",
                  help="Monitoring interval. If not set loop can be controlled by SIGUSR1 or SIGUSR2", metavar="INTERVAL")
parser.add_argument("-b", "--binary", dest="binary",
                  help="Binary to be executed", metavar="BIN")
parser.add_argument("-d", "--directory", dest="directory", 
                  help="Directory to store output data", metavar="DIR")

args = parser.parse_args()

if None in [args.cgroup, args.limit, args.binary]:
  parser.error("Incorrect number of arguments. Exit...")
  sys.exit()

# Set standard values for malloc hook, if not set
if os.environ.has_key("MALLOC_INTERPOSE_OUTFILE") == False:
  os.environ["MALLOC_INTERPOSE_OUTFILE"] = str(os.getcwd() + "/" + args.directory + "/mallocOutput_%p.fom")
if os.environ.has_key("MALLOC_INTERPOSE_DEPTH") == False:
  os.environ["MALLOC_INTERPOSE_DEPTH"] = "100"
if os.environ.has_key("MALLOC_INTERPOSE_SHIFT") == False:
  os.environ["MALLOC_INTERPOSE_SHIFT"] = "10"

# Setup a cgroup subgroup 
try:
  os.mkdir(cgroupPath + "/memory/"+args.cgroup+"/FOM")
except:
  print "Memory Cgroup FOM already exist."
try:
  os.mkdir(cgroupPath + "/freezer/"+args.cgroup+"/FOM")
except:
  print "Freezer Cgroup FOM already exist."

# Set memory limit
cmd = 'echo ' + args.limit + ' > ' + cgroupPath + '/memory/' + args.cgroup + '/FOM/memory.limit_in_bytes'
call(cmd, shell=True)

# Start process
try:
  environment = os.environ.copy()
  fdir = os.path.dirname(os.path.realpath(__file__))[:-6]
  if os.environ.has_key("LD_LIBRARY_PATH") :
      for path in os.environ["LD_LIBRARY_PATH"].split(":"):
          if os.path.isfile(path+"/libMallocHook.so"):
              lmpath=path
              break
          
  else:
      print "Cannot find libMallocHook.so. please add its path to LD_LIBRARY_PATH envionment"
      sys.exit()
  environment['LD_PRELOAD'] = lmpath+"/libMallocHook.so"
  cmd = shlex.split(args.binary)
  fd = open("output", "w")
  p = Popen(cmd, env=environment,  stdout=fd, stderr=fd)
except:
  print "A problem occured while starting ", args.binary , " - Exit..." 
  sys.exit()

# Put process into memory and freezer cgroup
cmd = 'echo ' + str(p.pid) + ' > ' + cgroupPath + '/memory/' + args.cgroup + '/FOM/tasks'
call(cmd, shell=True)

cmd = 'echo ' + str(p.pid) + ' > ' + cgroupPath  + '/freezer/' + args.cgroup + '/FOM/tasks'
call(cmd, shell=True)

# Change to working directory (if it is set)
if args.directory != None:
  try:
    os.chdir(args.directory)
  except:
    print "Could not find ", args.directory, "- directoy. Will write output to ", os.getcwd()
else:
  args.directory = ''

# Function to determine address range of heap and to read pagemap
iteration = 0
max = 0
outputFiles = []
heapsizes = []

def getSnapshot():
  global iteration
  global max
  global heapsizes
  global outputFiles
  global pagesize

  t = time.time()
  iteration = iteration + 1
  file = open("/proc/"+str(p.pid)+"/maps", "r")
  if file == None:
    return
  print "Iteration:", iteration, "\n\tFreezing process" 
  cmd = 'echo FROZEN > ' + cgroupPath + '/freezer/' + args.cgroup + '/FOM/freezer.state'
  call(cmd, shell=True)
  
  # Make sure FROZEN state is really set 
  tmp = cgroupPath + '/freezer/' + args.cgroup + '/FOM/freezer.state'
  killSwitch = False
  while(True):
    time.sleep(0.001)
    f2 = open(tmp)
    if "FROZEN" in f2.readline():
        break
    else:
      if (time.time() - t) > 10: #something went wrong with cgroup - deadlock
	if not killSwitch: #Try again to freeze
          cmd = 'echo FROZEN > ' + cgroupPath + '/freezer/' + args.cgroup + '/FOM/freezer.state'
          call(cmd, shell=True)
	  killSwitch=True
	else:
	  print "It seems Cgroup ended up in a deadlock - Aborting"
          sys.exit()

  # Read status of pages on the heap
  t2 = time.time()
  hasHeap = False # when first snapshot is made heapsize could be still 0
  for i in file.readlines():
     if "[heap]" in i:
        hasHeap = True
        pageRange = i.split() [0]
        heap  = pageRange.split("-")
        swapped = PageStatusChecker.analyze(int(p.pid), iteration, "iteration%04d"%iteration,  heap[0], heap[1])
        heapsize = (int(heap[1],16) - int(heap[0],16))  / pagesize
        heapsizes.append(heapsize)
        if (heapsize) > max:
           max = heapsize
        break
  t3 = time.time()

  # Unfreeze
  cmd = 'echo THAWED > ' + cgroupPath + '/freezer/' + args.cgroup + '/FOM/freezer.state'
  call(cmd, shell=True)

  # Make sure THAWED state is really set 
  while(True):
    time.sleep(0.001)
    f2 = open(tmp)
    if "THAWED" in f2.readline():
        break
    else:
      if (time.time() - t) > 10: #something went wrong with cgroup - deadlock
        if not killSwitch: #Try again to unfreeze
          cmd = 'echo THAWED > ' + cgroupPath + '/freezer/' + args.cgroup + '/FOM/freezer.state'
          call(cmd, shell=True)
          killSwitch=True
        else:
          print "It seems Cgroup ended up in a deadlock - Aborting"
          sys.exit()

  if hasHeap == False:
    iteration = iteration - 1 #reset iteration if no snapshot was made
  else:
    outputFiles.append("iteration%04d"%iteration)

  print "\t", time.time() - t, "secs for making snapshot\n\t", t3 - t2, "secs for reading pagemap" 

# Monitoring loop: if args.interval not set, then snapshots have to be triggered by SIGUSR
if args.interval == None:
  # Signal handler for user defined snapshots
  def signalHandlerSIGUSR(signal, frame):
    getSnapshot()
    print "Run process until next SIGUSR is received"

  signal.signal(signal.SIGUSR1, signalHandlerSIGUSR)
#  signal.signal(signal.SIGUSR2, signalHandlerSIGUSR)
  
  while (p.poll() == None):
     time.sleep(1)

else:    
  # Start standard monitoring loop
  while (p.poll()==None):
    getSnapshot()
    print "\tRun process for " + args.interval + " seconds"
    time.sleep(int(args.interval))
print "Process finished. Perform cleanup and write summary."
# Clean up
try: 
  os.rmdir(cgroupPath + "/memory/" + args.cgroup + "/FOM")
  os.rmdir(cgroupPath + "/freezer/" + args.cgroup + "/FOM")
except:
  pass

#write Summary
dict ={"Executed Command": args.binary, "Memory Limit": args.limit, 
       "Interval": args.interval, "Stacktrace Depth":  os.environ["MALLOC_INTERPOSE_DEPTH"], 
       "Interpose Shift": os.environ["MALLOC_INTERPOSE_SHIFT"], 
       "Maximum Heapsize": max,"Number Of Output Files": iteration, 
       "Pid": p.pid, "Malloc Output File": os.environ["MALLOC_INTERPOSE_OUTFILE"], 
       "Heap Sizes": heapsizes, "Output Files": outputFiles }
with open('FOMSummary.json', 'w') as fp:
    json.dump(dict, fp)
