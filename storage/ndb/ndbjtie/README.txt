
XXX Martin: needs to be updated
#if 0

NDB JTie -- A Robust Java Wrapper for NDB API
---------------------------------------------

Directory Structure:

ndbjtie/				this directory

    jtie/				JTie generic C++ template library
	include/			API & implementation sources
	src/com/mysql/jtie/		Java sources
	test/				unit tests

    mysql/				MySQL Utility function library
	include/
        src/
        test/

    ndbjtie/				NDB Java API library
        src/
	    com/mysql/ndbjtie/mgmapi/	Java Management API
	    com/mysql/ndbjtie/mysql/	MySQL Utilities functions
	    com/mysql/ndbjtie/ndbapi/	Java NDB API
	    mgmapi.cpp			MGM API JNI implementation
	    mysql.cpp			MySQL Utilities JNI implementation
	    ndbapi.cpp			NDB API JNI implementation
        test/				unit tests

    utils/				utility classes (for debugging)


The current, provisional build system
-------------------------------------

    env.properties			external include & tool dependencies
    Makefile				global targets
    Makefile.defaults			default rules & targets

Quick build & test procedure:

- edit env.properties for the JAVA and NDBAPI include/lib paths

- run "make help" for a list of gloabl make targets

- run "make dep" to build the dependencies in all subprojects

- run "make dbg" to build the debug binaries in all subprojects

- in jtie/test, run "make check" for the generic lib unit test

- in ndbjtie/test, run "make check" for the NDB JTIE "smoke" test
  (requires a running cluster on default ports with CRUND schema loaded)


Questions and comments appreciated.
martin.zaun@sun.com


To Do List
----------

- mapping support for Object[] parameters

- current version ~0.9

- integrate with automake, Cluster paths, build system

- cleanup and fix mapping issues tagged by "// MMM! ..."

- cleanup and fix code issues tagged by    "// XXX ..."

- uncomment & implement unmapped NDB API functions needed by Cluster/J

- ...

#endif
