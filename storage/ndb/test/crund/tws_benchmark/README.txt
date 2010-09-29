
TWS (Table With Strings)
------------------------

A self-contained benchmark that measures the performance of selected MySQL
Server/Cluster APIs for basic operations on a string/varchar-based schema.

This benchmark may be integrated into CRUND, in future.

Questions or comments appreciated: martin.zaun@oracle.com


How to Build, Run, and Profile TWS
----------------------------------

The benchmark is built and run by Ant scripts (generated from Netbeans).

0) Configure TWS Properties

    The benchmark is built and run by Ant scripts (originally, generated
    from Netbeans).

    Copy the configuration sample files
        $ cd ./nbproject

        $ cp -vr configs_sample configs
        configs_sample -> configs
        configs_sample/server-dbg.properties -> configs/server-dbg.properties
        configs_sample/server-opt.properties -> configs/server-opt.properties

        $ cp -vr private_sample private
        private_sample -> private
        private_sample/config.properties -> private/config.properties
        private_sample/private.xml -> private/private.xml
        private_sample/profiler -> private/profiler
        private_sample/profiler/configurations.xml -> private/profiler/configurations.xml
        private_sample/private.properties -> private/private.properties

    Edit the files
        ./configs/server-dbg.properties
        ./configs/server-opt.properties
        ./private/private.properties
    for the paths to TWS's external software dependencies.

    Edit the file
        ./private/config.properties
    to select the JVM run configuration (debug/optimized settings).


1) Build TWS

    Standard project targets:
        $ ant default
        $ ant compile
        $ ant clean
        $ ant jar
        $ ant javadoc
        $ ant deps-jar
        $ ant deps-clean


2) Run TWS

    Copy the run configuration sample file
	$ cp -v run.properties.sample run.properties
	run.properties.sample -> run.properties

    Edit the file for the benchmark settings
        ./run.properties

    Run (and Profiling?) targets:
        $ ant run

    At this time, all the benchmark's data output is to stdout.
