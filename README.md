FOM Tools provide utilities to Find Obsolete Memory. The aim is to find pages which move to swap and remain there. Such pages contain objects which are not used any longer by an application and as a consequence the memory has become obsolete and could be potentially released. When objects are allocated through external libraries, then often a clean up is performed by them in the end of an application's lifetime. Consequently, such objects will never be detected as a memory leak. However, FOM allows to detect these problematic memory allocations. The tools consist of three parts:

    - preprocessing to evaluate the right memory limit for an application. The limit should be such that the application does not start thrashing.
    - monitoring utility to track page status and malloc calls
    - postprocessing tools to map objects to swapped pages, to plot and merge the data and to make statistics 

Requirements:

    - Numpy
    - Matplotlib (> version 1.2.0)
    - compiler with C++11 support
    - libunwind
    - Cgroup (Memory/Freezer) 

What can be analyzed with FOM:

    - Which pages were in Swap, RAM or have never been loaded
    - Memory utilization patterns
    - Memory hotspots
    - Time allocation profiles
    - How much of large datasets remains in Swap/RAM
    - Which function made how many allocations
    - Which allocation was made by which function
    - Depth of stacktraces 

How to configure and install:
    mkdir ../build
    cd ../build
    cmake -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_C_COMPILER=$(which gcc) ../FOM-tools -DCMAKE_INSTALL_PREFIX=${PWD}/../install
    make
    
Monitoring:
 The following command starts the application and tracks the page status:
    Monitor.py --interval=60 --cgroup=test --directory=testrun --limit=1500M --binary=”/usr/bin/python test.py”

 It takes the following arguments:

    - interval: time to wait in seconds before the next snapshot will be taken. If interval is not set, the script expects SIGUSR
    - cgroup: the name of the cgroup to which the process shall be assigned. It needs to have the permission of the user. The script will then create a subgroup FOM in this cgroup. Howto setup cgroup: sudo cgcreate -a USERNAME -g freezer,memory:test or have a look at: https://twiki.cern.ch/twiki/bin/view/ITSDC/ProfAndOptExperimentsApps#Linux_Control_Groups
    - directory: where the output files will be written
    - limit: when the process shall start swapping
    - binary: command to be executed as subprocess. It will inherit the environment from the parent shell. 

The malloc hook can be configured with the following environment variables:

    - MALLOC_INTERPOSE_SHIFT defines minimum size of allocation which will be tracked (default: 10 = 8 kB)
    - MALLOC_INTERPOSE_DEPTH defines the maximum stacktrace depth which will be analyzed (default: 100 - in CERN applications stacktraces can easily have a depth of 70 due to the Python-C++ layer)
    - MALLOC_INTERPOSE_OUTFILE defines the outputfile (default: mallocOutputFile%p - the malloc hook will replace %p with the corresponding pid) 

If __DO_GNU_BACKTRACE__ is set, then glibc backtrace() is used if not it uses libwunind 

Postprocessing:
  Some basic analysis workflows are provided and scripts are loacted under FOM-tools/postprocessing. These workflows can be easily extended or adjusted. Following scripts are currently provided: 
  - GeneratePlot.py
     - Reads all output files with the name iteration and generates graphical output (png format)
     - Defines gnuplot command, sets correct resolution, set ymin and xmax etc.
     - yaxis = different iterations
     - xaxis = pages on the heap
     - Red pixel: page in swap
     - Green pixel: page in RAM
     - Grey pixel: page not loaded  
     - The resolution can lead to problems for standard image viewers: We advise using nip2 in order to load the generated png-file. See further details about nip2 here http://www.vips.ecs.soton.ac.uk/index.php?title=Downloading,_Installation_and_Startup_of_Nip2
 -  PlotHistograms.py
     - Analyzes blocks of continuous pages in RAM/Swap/not loaded
     - Plots for each iteration a histogram with the distribution of these blocks (xaxis in logscale)
     - The scirpt makes use of the animation module of matplotlib and requires matplotlib versions > 1.2.0

 -  Postprocessing.py
    Script consists of several steps:
      - Preparing output files:
         -  Merge and sort output files
         -  Map function offsets in the symbol lookup table to sourcelines: This is currently done with eu-addr2line thats is part of the elfutils package.
    Actual postprocessing:
      - Find top10 biggest blocks of continuous pages in swap/RAM/not loaded
      - Get corresponding stacktraces for the 10 biggest blocks in swap. Results are written to separate outputfiles (Hotspot0-Hotspot9)
      - Find number of pages always in RAM and most of the time in swap 
      
