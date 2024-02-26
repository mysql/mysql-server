# ==== Purpose ====
#
# Assert that both super_read_only and read_only variables are
# enabled and reset their value.
#
#
# ==== Usage ====
#
# [--let $skip_reset_read_mode= 1]
# [--let $rpl_debug= 1]
# --source include/assert_and_disable_read_only.inc
#
# Parameters:
#   $skip_reset_read_mode
#     Skip reset both super_read_only and read_only variables.
#
#   $rpl_debug
#     See include/rpl_init.inc

--let $include_filename= assert_and_disable_read_only.inc
--source include/begin_include_file.inc

if (!$rpl_debug)
{
  --disable_query_log
}

--let $assert_cond= "[SELECT @@GLOBAL.super_read_only]" = 1
--let $assert_text= Assert that server is running in super read only.
--source include/assert.inc
--let $assert_cond= "[SELECT @@GLOBAL.read_only]" = 1
--let $assert_text= Assert that server is running in read only.
--source include/assert.inc

if (!$skip_reset_read_mode)
{
  # Disable super read only.
  SET GLOBAL read_only= FALSE;
}
--let $skip_reset_read_mode=

--let $include_filename= assert_and_disable_read_only.inc
--source include/end_include_file.inc
