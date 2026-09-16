# ==== Purpose ====
#
# Ensure that the CHANGE REPLICATION SOURCE TO APPLIER_VERSION=2 can be used.
#
# ==== Usage ====
#
# --let $rpl_master= <server id>
# --let $use_gtids= <1 if replica is configured with SOURCE_AUTO_POSITION = 1>
# --source include/rpl/have_csa_requirements.inc

if ($rpl_master == '')
{
  --die ERROR: You must set $rpl_master before sourcing have_csa_requirements.inc
}

# Ensure that source is configured to use row format.
--let $_saved_connection= $CURRENT_CONNECTION
--connection server_$rpl_master
--let $_have_binlog_format_row= `SELECT @@binlog_format LIKE 'ROW'`
--connection $_saved_connection
if (!$_have_binlog_format_row)
{
  --skip CHANGE REPLICATION SOURCE TO APPLIER_VERSION=2 requires binlog_format = ROW on the source server
}

--let $_tmp_gtid_mode= `SELECT @@GLOBAL.GTID_MODE`
if ($_tmp_gtid_mode != 'ON')
{
  --skip CHANGE REPLICATION SOURCE TO APPLIER_VERSION=2 requires gtid_mode = ON
}

if (!$use_gtids)
{
  --skip CHANGE REPLICATION SOURCE TO APPLIER_VERSION=2 requires SOURCE_AUTO_POSITION = 1
}

