# ==== Purpose ====
#
# Check if the machine has 64 bits word size.
#
# Note: Windows platforms can have 32 bit ulong even if the machine is
# 64 bit. So be sure to choose the correct condition for your test!
# Tests that check the range of system variables whose maximum is
# ULONG_MAX will depend on the ulong size, rather than the machine
# word size.
#
# ==== Usage ====
#
# --source include/check_64bit_machine.inc
# # Now $have_64bit_machine is 0 if ulong is 32 bits, and 1 if ulong is 64 bits.
#
# ==== See also ====
#
# - have_64bit_ulong.inc/have_64bit_ulong.inc/check_32bit_ulong.inc

--disable_query_log
--disable_warnings
--let $_c64m_myisam_sort_buffer_size_save = `SELECT @@session.myisam_sort_buffer_size`
SET @@session.myisam_sort_buffer_size = 4294967296;
--let $have_64bit_machine = `SELECT @@session.myisam_sort_buffer_size > 4294967295`
eval SET @@session.myisam_sort_buffer_size = $_c64m_myisam_sort_buffer_size_save;
--enable_warnings
--enable_query_log
