# ==== Purpose ====
#
# Check if the platform uses 64 bits for ulong.
#
# Note: Windows platforms can have 32 bit ulong even if the machine is
# 64 bit. So be sure to choose the correct condition for your test!
# Tests that check the range of system variables whose maximum is
# ULONG_MAX will depend on the ulong size, rather than the machin word
# size.
#
# ==== Usage ====
#
# --source include/check_64bit_ulong.inc
# # Now $have_64bit_ulong is 0 if ulong is 32 bits, and 1 if ulong is 64 bits.
#
# ==== See also ====
#
# - have_32bit.inc/have_64bit.inc/check_32bit_machine.inc
# - have_64bit_ulong.inc/have_64bit_ulong.inc/check_32bit_ulong.inc

--disable_query_log
--disable_warnings
--let $_c64u_min_examined_row_limit_save = `SELECT @@session.min_examined_row_limit`
SET @@session.min_examined_row_limit = 4294967296;
--let $have_64bit_ulong = `SELECT @@session.min_examined_row_limit > 4294967295`
eval SET @@session.min_examined_row_limit = $_c64u_min_examined_row_limit_save;
--enable_warnings
--enable_query_log
