FOM Tools provide utilities to Find Obsolete Memory. The aim is to find pages which move to swap and remain there. Such pages contain objects which are not used any longer by an application and as a consequence the memory has become obsolete and could be potentially released. 

The tools consist of three parts:
- preprocessing to evaluate the right memory limit for an application. The limit should be such that the application does not start thrashing.
- monitoring utility to track page status and malloc calls
- postprocessing tools to map objects to swapped pages, to plot and merge the data and to make statistics 

Requirements:
- Numpy
- Matplotlib (> version 1.2.0)
- compiler with C++11 support
- libunwind
- Cgroup (Memory/Freezer) 

How to configure and install:

    mkdir ../build
    cd ../build
    cmake -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_C_COMPILER=$(which gcc) ../FOM-tools -DCMAKE_INSTALL_PREFIX=${PWD}/../install
    make

A detailed description about the tool can be found here: https://gitlab.cern.ch/fom/FOM-tools/wikis/home