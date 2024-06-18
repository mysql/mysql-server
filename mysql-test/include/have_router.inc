# Including this file will include the MySQLRouter suite. As a result 10 TCP
# ports from the pool gets reserved for the Router use.
#
# The Router executable is not launched via my.cnf section ([router.1]) as
# other excuatables. This is because the Router does not support the same
# configuration file format.
#
# If this file is included the Router process will be launched with
# configuration created from a template '<testcase_name>-router.cnf',
# or '<suite_name>-router.cnf' if missing, or 'default_my-router.cnf'
# if missing.
