# This suite contains tests that require special hardware configuration.
# Please run it only on machines that satisfy conditions specified in test.
################################################################################
* innodb_dedicated-server - This has to be run on a machine that has only one mysqld.
  This test suite can't run with MTR --parallel > 1 or if there are other mysqld instances on machine.
