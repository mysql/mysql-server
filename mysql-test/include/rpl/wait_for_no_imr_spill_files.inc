# ==== Purpose ====
#
# Wait until the in-memory relay log spill directory holds no spill files
# (basename pattern "imr_sp_*"). A committed spill transaction's file is deleted
# when its byte source is released (commit + applier job teardown), which is
# asynchronous to the transaction's GTID becoming visible on the replica. This
# helper polls the directory so tests can deterministically assert that spill
# files are reclaimed after commit.
#
# The enclosing "in_memory_relaylog_temp_files" directory itself persists and is
# not removed by this check.
#
# ==== Usage ====
#
# --let $spill_dir_to_check= /path/to/.../in_memory_relaylog_temp_files
# [--let $spill_wait_timeout= 30]   # seconds; default 30
# --source include/rpl/wait_for_no_imr_spill_files.inc

--let $include_filename= rpl/wait_for_no_imr_spill_files.inc
--source include/begin_include_file.inc

if ($spill_dir_to_check == '')
{
  --die !!!ERROR IN TEST: you must set $spill_dir_to_check before sourcing wait_for_no_imr_spill_files.inc
}

--let $_imr_spill_timeout= $spill_wait_timeout
if ($_imr_spill_timeout == '')
{
  --let $_imr_spill_timeout= 30
}
# Poll at 0.1s granularity.
--let $_imr_spill_tries= `SELECT $_imr_spill_timeout * 10`

--let $_imr_spill_ls_file= $MYSQL_TMP_DIR/imr_spill_ls.txt
--let $_imr_spill_files= not_empty_yet
while (`SELECT '$_imr_spill_files' <> ''`)
{
  --list_files_write_file $_imr_spill_ls_file $spill_dir_to_check imr_sp_*
  --let $read_from_file= $_imr_spill_ls_file
  --let $include_silent= 1
  --source include/read_file_to_var.inc
  --let $include_silent=
  --remove_file $_imr_spill_ls_file
  --let $_imr_spill_files= $result

  if (`SELECT '$_imr_spill_files' <> ''`)
  {
    --dec $_imr_spill_tries
    if (!$_imr_spill_tries)
    {
      --echo Leftover spill files in $spill_dir_to_check:
      --echo $_imr_spill_files
      --die Timed out waiting for in-memory relay log spill files to be deleted
    }
    --sleep 0.1
  }
}

--echo # No spill files remain in the in-memory relay log temp directory.

--let $include_filename= rpl/wait_for_no_imr_spill_files.inc
--source include/end_include_file.inc
