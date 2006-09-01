
Unit tests directory structure
------------------------------

This is the current structure of the unit tests.  More directories
will be added over time.

mytap                 Source for the MyTAP library
mysys                 Tests for mysys components
  bitmap-t.c          Unit test for MY_BITMAP
  base64-t.c          Unit test for base64 encoding functions
examples              Example unit tests
  simple-t.c          Example of a standard TAP unit test
  skip-t.c            Example where some test points are skipped
  skip_all-t.c        Example of a test where the entire test is skipped
  todo-t.c            Example where test contain test points that are TODO
  no_plan-t.c         Example of a test with no plan (avoid this)


Executing unit tests
--------------------

To make and execute all unit tests in the directory:

   make test


Adding unit tests
-----------------

Add a file with a name of the format "foo-t.c" to the appropriate
directory and add the following to the Makefile.am in that directory
(where ... denotes stuff already there):

  noinst_PROGRAMS = ... foo-t

Note, it's important to have "-t" at the end of the filename, otherwise the
test won't be executed by 'make test' !


Documentation
-------------

There is Doxygen-generated documentation available at:

      https://intranet.mysql.com/~mkindahl/mytap/html/
