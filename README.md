FOM Tools provide utilities to study the memory allocation patterns of
an application. It has two modes 
- Find Obsolete Memory. 
- Record memory (de-)allocation patterns

First mode aims to find pages which move to swap and
remain there. Such pages contain objects which are not used any longer
by an application and as a consequence the memory has become obsolete
and could be potentially released.

Obsolete memory tools consist of three parts:
- preprocessing to evaluate the right memory limit for an application. The limit should be such that the application does not start thrashing.
- monitoring utility to track page status and malloc calls
- postprocessing tools to map objects to swapped pages, to plot and merge the data and to make statistics 

Requirements:
- Numpy
- Matplotlib (> version 1.2.0)
- compiler with C++11 support
- libunwind
- Cgroup (Memory/Freezer)

Second mode interposes malloc/calloc/realloc and free calls and
records time, stacktrace and size. This information is then saved to a
record file for post processing. During the postprocessing step four metrics have been calculated.
- lifetime of allocation
- allocation density in unit time
- locality of allocated addresses
- variations in allocation sizes


How to configure and install:

    git clone https://github.com/FOM-Tools/FOM-Tools.git
    mkdir build
    cd build
    cmake -DCMAKE_CXX_COMPILER=$(which g++) -DCMAKE_C_COMPILER=$(which gcc) ../FOM-tools -DCMAKE_INSTALL_PREFIX=${PWD}/../install
    make
    make install

A detailed description about the tool can be found here: https://github.com/FOM-Tools/FOM-Tools/wiki