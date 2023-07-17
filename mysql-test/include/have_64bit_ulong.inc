# ==== Purpose ====
#
# Skip the test if ulong is not 64 bits.
#
# Note: this is *different* from have_64bit.inc.  Windows platforms
# can have 32 bit ulong even if the machine is 64 bit.
#
# ==== See also ====
#
# - check_32bit_machine.inc
# - have_64bit_ulong.inc/have_64bit_ulong.inc/check_32bit_ulong.inc

--source include/check_64bit_ulong.inc
if (!$have_64bit_ulong)
{
  --skip Requires 64 bit ulong
}
