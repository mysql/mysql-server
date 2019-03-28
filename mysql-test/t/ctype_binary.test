--source include/no_valgrind_without_big.inc

set names binary;

--echo #
--echo # Start of 5.5 tests
--echo #

--source include/ctype_numconv.inc

--echo #
--echo # End of 5.5 tests
--echo #

--echo #
--echo # Start of 5.6 tests
--echo #

#
# Bugs#12635232: VALGRIND WARNINGS: IS_IPV6, IS_IPV4, INET6_ATON,
# INET6_NTOA + MULTIBYTE CHARSET.
#

SET NAMES binary;
--source include/ctype_inet.inc

--echo #
--echo # End of 5.6 tests
--echo #
