# ==== Purpose ====
#
# Skip the test if the machine word size is not 32 bit.
#
# Note: This is not equivalent to ULONG_MAX==INT32_MAX.  On Windows,
# even a machine with 64 bit words may have 32 bit ulong.  If your
# test depends on the size of ulong, use have_32bit_ulong.inc instead.
#
# ==== See also ====
#
# - have_32bit.inc/have_64bit.inc/check_32bit_machine.inc
# - have_64bit_ulong.inc/have_64bit_ulong.inc/check_32bit_ulong.inc

--source include/check_64bit_machine.inc
if ($have_64bit_machine)
{
  --skip Requires a 32 bit machine
}
--let $mach32 = 1
