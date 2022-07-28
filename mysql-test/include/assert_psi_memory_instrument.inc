###############################################################################
# Verify Event Names for memory instrument can be:
# - enabled/disabled in starting
# - disabled/enabled by updating ENABLED column of setup_instruments table.
#
# ==== Usage ====
#
#   --let $ps_event_name= Event name
#   [--let $ps_event_name_status= 1|0]
#   --source include/assert_psi_memory_instrument.inc
#
# Parameters:
#   $ps_event_name
#     the memory instrument full Event name.
#     Example: memory/group_rpl/transaction_data
#
#   $ps_event_name_status
#     If $ps_event_name_status == 1 then it test the memory instrument:
#     - enabled in starting
#     - can be disabled by updating ENABLED column of setup_instruments table.
#     If $ps_event_name_status == 0 then it test the memory instrument:
#     - disabled in starting
#     - can be enabled by updating ENABLED column of setup_instruments table.
#
#
# Test:
# 1. Verify that event_name is not empty
# 2. Verify that the Event name memory instrument is enabled/disabled.
# 3. Verify that the Event name memory instrument
#    can be disabled/enabled by updating ENABLED column of
#    setup_instruments table.
# 4. Cleanup
###############################################################################

# 1. Verify that event_name is not empty
if ($ps_event_name == '')
{
  --die ERROR IN TEST: the "ps_event_name" variable must be set `
}

if ($ps_event_name_status == '')
{
  --let $ps_event_name_status=1
}

if ($ps_event_name_status==1)
{
  --let $_ps_event_name_status_text=YES
  --let $_ps_event_name_status_opposite_text=NO
}

if ($ps_event_name_status==0)
{
  --let $_ps_event_name_status_text=NO
  --let $_ps_event_name_status_opposite_text=YES
}


# 2. Verify that the Event name memory instrument is enabled/disabled.
--let $assert_text= 'There should be 1 entry corresponding to the $ps_event_name in the setup_instruments table for ENABLED=$_ps_event_name_status_text'
--let $assert_cond= COUNT(*) = 1 FROM performance_schema.setup_instruments WHERE NAME LIKE "$ps_event_name" and ENABLED="$_ps_event_name_status_text"
--source include/assert.inc


# 3. Verify that the Event name memory instrument can be disabled by updating
#    ENABLED column of setup_instruments table.
--eval UPDATE performance_schema.setup_instruments SET ENABLED='$_ps_event_name_status_opposite_text' WHERE NAME LIKE "$ps_event_name";

--let $assert_text= 'There should be 1 entry corresponding to the $ps_event_name in the setup_instruments table for ENABLED=$_ps_event_name_status_opposite_text'
--let $assert_cond= COUNT(*) = 1 FROM performance_schema.setup_instruments WHERE NAME LIKE "$ps_event_name" and ENABLED="$_ps_event_name_status_opposite_text"
--source include/assert.inc


# 4. Cleanup
--eval UPDATE performance_schema.setup_instruments SET ENABLED='$_ps_event_name_status_text' WHERE NAME LIKE "$ps_event_name";
