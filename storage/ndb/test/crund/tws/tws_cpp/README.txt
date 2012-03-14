
How to Build and Run the C++ TWS Benchmark
------------------------------------------

This benchmark is being built with
-> Gnu 'make' (using generic, hand-coded Makefiles)
-> Gnu g++    (or Solaris Studio CC as compilers)

0)  Configure external dependencies paths

    Edit the file
        ../../env.properties

    and configure these five properties (shared with CRUND):

        MYSQL_HOME=${HOME}/mysql/bin-7.1-opt32
        TARGET_ARCH=-m32

        NDB_INCLUDEOPT0=-I${MYSQL_HOME}/include/mysql/storage/ndb
        NDB_INCLUDEOPT1=-I${MYSQL_HOME}/include/mysql/storage/ndb/ndbapi
        NDB_LIBDIR=${MYSQL_HOME}/lib/mysql

    Important: TARGET_ARCH must match the MYSQL_HOME binaries.

1)  Build the binary

    Run once to build the include dependencies:
        $ make dep

    Build either a Release or Debug binary:
        $ make opt
        $ make dbg

    For a list of available targets:
        $ make help

2)  Run the benchmark

    Edit the benchmark's settings (copy from ../run.properties.sample)
        ../run.properties

    Have a running NDB Cluster with loaded schema file: ../schema.sql.

    Run the benchmark:
        $ make run.driver
    or
        $ ./TwsDriver -p ../run.properties

    The benchmark's result data is written to a generated log_XXX file.

Comments or questions appreciated.
martin.zaun@oracle.com
