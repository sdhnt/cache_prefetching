Paths to the files with code skeleton:
/afs/inf.ed.ac.uk/group/teaching/car/assignment2/tools/pin-3.5-97503-gac534ca30-gcc-linux/source/tools/PrefetcherExample/prefetcher_example.cpp
/afs/inf.ed.ac.uk/group/teaching/car/assignment2/tools/pin-3.5-97503-gac534ca30-gcc-linux/source/tools/PrefetcherExample/dcache_for_prefetcher.hpp

To compile this files please follow the instructions below.

###########################################################################

# How to launch benchmarks?
Firstly, source shrc
source /afs/inf.ed.ac.uk/group/teaching/car/assignment2/shrc

There is one micro-benchmark; the following is the command to launch it:

$BENCH_PATH/microBench.exe


###########################################################################

# How to compile the code and use Pin?
1. Source shrc
source /afs/inf.ed.ac.uk/group/teaching/car/assignment2/shrc

2. Compile your tool (with a preftecher)
A Pin tool is a dynamic shared library (file with .so filename extension).

NOTE: You don't have write permissions on CAR shared filespace space.
      In order to compile your code, please copy the whole pin directory to your home directory.

Copy pin directory to your DICE home and compile a tool with the following commands:
mkdir -p $HOME/car/
cp -r $CAR/assignment2/tools/pin-3.5-97503-gac534ca30-gcc-linux $HOME/car/
PREF_EXAMPLE=$HOME/car/pin-3.5-97503-gac534ca30-gcc-linux/source/tools/PrefetchExample
cd $PREF_EXAMPLE
make -B obj-intel64/prefetcher_example.so TARGET=intel64

Last command will create prefetcher_example.so in the directory $PREF_EXAMPLE/obj-intel64.
The file prefetcher_example.so is a tool which implements a next line prefetcher.

To open a file you just compiled use
gedit $PREF_EXAMPLE/prefetcher_example.cpp

In order to create your own tool, you need to change the file $PREF_EXAMPLE/prefetcher_example.cpp
and recompile the tool with the following command:
make -B obj-intel64/prefetcher_example.so TARGET=intel64
After this the tool $PREF_EXAMPLE/obj-intel64/prefetcher_example.so will be updated with your changes.
NOTE: 'make' should be launched from $PREF_EXAMPLE directory
NOTE: the argument -B allows you to also modify and compile the header file dcache_for_prefetcher.hpp, however there is no need to do so.

3. Run Pin with the tool
pin -t <tool> <tool options> -- <benchmark>

Any of three benchmarks defined above can be used as a <benchmark> here. For example

pin \
  -t $PREF_EXAMPLE/obj-intel64/prefetcher_example.so                                       \
  -pref_type next_n_lines -aggr 1 -o output.out                                           \
  --  $BENCH_PATH/microBench.exe



