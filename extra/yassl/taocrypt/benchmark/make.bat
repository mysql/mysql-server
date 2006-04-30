# quick and dirty build file for testing different MSDEVs
setlocal 

set myFLAGS= /I../include /I../../mySTL /c /W3 /G6 /O2
#set myFLAGS= /I../include /I../../mySTL /c /W3 

cl %myFLAGS% benchmark.cpp

link.exe  /out:benchmark.exe ../src/taocrypt.lib benchmark.obj

