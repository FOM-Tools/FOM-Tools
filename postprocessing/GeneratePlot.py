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

# Simple script to generate graphical output

import os
import sys
import resource
import json
from subprocess import Popen, call, PIPE

if os.path.isfile("FOMSummary.json"):
  f = open('FOMSummary.json')
  dict = json.loads(f.read())
  max = dict["Maximum Heapsize"]
  files = dict["Output Files"]
else:
 print "   Cannot find FOMSummary.json"
 sys.exit()

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
  cmd += "'iteration%04d'"%i + " using 3:1:4  w dots lc variable," 
cmd += " 'iteration%04d'"%(i+1) + " using 3:1:4  w dots lc variable\n" 

# Write file
f = open("cmd.txt", "w")
f.write(cmd)
f.close()

# Execute Gnuplot and perform cleanup
call(["gnuplot", "cmd.txt"])
#call(["rm", "cmd.txt"])