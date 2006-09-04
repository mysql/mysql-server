
Unit tests directory structure
------------------------------

This is the current structure of the unit tests.  More directories
will be added over time.

mytap                 Source for the MyTAP library
mysys                 Tests for mysys components
  bitmap-t.c          Unit test for MY_BITMAP
  base64-t.c          Unit test for base64 encoding functions
examples              Example unit tests.
  core-t.c            Example of raising a signal in the middle of the test
		      THIS TEST WILL STOP ALL FURTHER TESTING!
  simple-t.c          Example of a standard TAP unit test
  skip-t.c            Example where some test points are skipped
  skip_all-t.c        Example of a test where the entire test is skipped
  todo-t.c            Example where test contain test points that are TODO
  no_plan-t.c         Example of a test with no plan (avoid this)


Executing unit tests
--------------------

To make and execute all unit tests in the directory:

   make test

Observe that the tests in the examples/ directory are just various
examples of tests and are not expected to pass.


Adding unit tests
-----------------

Add a file with a name of the format "foo-t.c" to the appropriate
directory and add the following to the Makefile.am in that directory
(where ... denotes stuff already there):

  noinst_PROGRAMS = ... foo-t

Note, it's important to have "-t" at the end of the filename, otherwise the
test won't be executed by 'make test' !

