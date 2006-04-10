

Unit test structure
-------------------

This is the current structure of the unit tests.  All directories does
not currently exist, and more directories will be added over time.

+ mysys                 Tests for mysys components
+ examples              Example unit tests
+ sql                   Unit tests for server code
  + rpl                 Unit tests for replication code
  + log                 Unit tests for logging

Executing unit tests
--------------------

To make and execute all unit tests in the directory:

   make test


Adding unit tests
-----------------

Add a file with a name of the format "foo.t.c" to the appropriate
directory and add the following to the Makefile.am in that directory
(where ... denotes stuff already there):

  noinst_PROGRAMS = ... foo.t
  foo_t_c_SOURCES = foo.t.c


