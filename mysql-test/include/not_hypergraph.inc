# include/not_hypergraph.inc
#
# Skip the test if we are forcing the hypergraph optimizer. This is typically
# because the test expects some given query plan, or more generally expects
# certain debug/non-critical input that will differ. Tests that give the wrong
# answer or crash under the hypergraph optimizer should not be using this file;
# instead, the optimizer should error out with ER_HYPERGRAPH_NOT_SUPPORTED_YET.
#

if ($HYPERGRAPH_TEST) {
  --skip Not supported under '--hypergraph'
}


