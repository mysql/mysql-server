# include/have_valgrind.inc
#
# If some test should be run with only valgrind then skip it while running test
# without it.
#

if (!$VALGRIND_TEST) {
  --skip Need '--valgrind'
}


