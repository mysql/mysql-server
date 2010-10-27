
NDB JTie -- A Robust Java Wrapper for NDB API
---------------------------------------------


src/
   demo, demoj		a stand-alone JNI demo
   jtie			the JTie generic Java wrapper template library
   myapi,  myjapi	the tests for JTie
   ndbjtie		the NDB JTie Java wrapper and test for NDB API


The demo/demoj (C++/Java) builds and runs independently from JTie & rest.

The jtie subdir has the generic Java wrapper template lib.  (Please, keep
in mind that this is still a prototype version 0.5, see To Do list below).

The myapi/myjapi is the current test api for jtie and builds/runs
independently from MySQL Cluster.

The ndbjtie is the NDBAPI wrapper test prototype and builds against the
NDBAPI headers; the java test program connects to a Cluster (running on the
default ports), gets an Ndb object, closes everything again and exits.


Quick install procedure:
- edit env.properties for the JAVA and NDBAPI include/lib dependencies
- run "make dep" to build the dependencies in all subprojects
- run "make dbg" to build the (debug) binaries in all subprojects
- in the src/* subprojects, run "make run.test" to run the local test

Questions and comments appreciated.
martin.zaun@sun.com

To Do:

- incorporate design decisions on how to map NDBAPI

- generate and adapt src/ndbjtie/ndbjtie/*.java based on API demarcated by
  DOXYGEN/NDBJTIE... flags 

- implement NDB JTie function stubs in src/ndbjtie/*.cpp

- expand NDB JTie test in src/ndbjtie/test/*.java

- refactorize generic template lib src/jtie for class mapping redundancies

- expand generic JTie test in src/myapi, src/myjapi

- rename package and directory paths to conventional/meaningful names

- integrate with automake

- integrate with Cluster paths, build system

- 
