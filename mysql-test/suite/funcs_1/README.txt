2008-02-29 Matthias Leich
=========================

1. The testsuite "funcs_1" is mostly intended for additional (compared
   to the common regression tests stored in mysql-test/t) checks
   of features (VIEWS, INFORMATION_SCHEMA, STORED PROCEDURES,...)
   introduced with MySQL 5.0.

2. There were some extensions of this suite when new information_schema
   views were introduced. But in most cases the tests for these views
   were stored within the regression testsuite (mysql-test/t).

   INFORMATION_SCHEMA views introduced with MySQL 5.1
   ==================================================
   ENGINES       (partially tested here)
   EVENTS        (partially tested here)
   FILES
   GLOBAL_STATUS
   GLOBAL_VARIABLES
   PARTITIONS
   PLUGINS
   PROCESSLIST   (full tested here)
   PROFILING
   REFERENTIAL_CONSTRAINTS
   SESSION_STATUS
   SESSION_VARIABLES

3. Some hints for maintainers of this suite:
   - SHOW TABLES ... LIKE '<pattern>'
     does a case sensitive comparison between the tablename and
     the pattern.
     The names of the tables within the informationschema are in uppercase.
     So please use something like
        SHOW TABLES FOR information_schema LIKE 'TABLES'
     when you intend to get the same non empty result set on OS with and
     without case sensitive filesystems and default configuration.
   - The name of the data dictionary is 'information_schema' (lowercase).
   - Server on OS with filesystem with case sensitive filenames
     (= The files 'abc' and 'Abc' can coexist.)
     + default configuration
     Example of behaviour:
     DROP DATABASE information_schema;
     ERROR 42000: Access denied for user ... to database 'information_schema'
     DROP DATABASE INFORMATION_SCHEMA;
     ERROR 42000: Access denied for user ... to database 'INFORMATION_SCHEMA'
   - Try to unify results by
     --replace_result $engine_type <engine_to_be_tested>
     if we could expect that the results for storage engine variants of a
     test differ only in the engine names.
     This makes future maintenance easier.
   - Avoid the use of include/show_msg*.inc.
     They produce "SQL" noise which annoys during server debugging and can be
     easy replaced by "--echo ...".

