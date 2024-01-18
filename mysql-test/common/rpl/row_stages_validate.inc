#
# Auxiliary include file
#
# Usage:
#
# --let $stage_thd_name= ...
# --let $state_stage= ...
# --let $state_db= ...
# --let $state_info= ...
# --let $state_command= ...
# --source common/rpl/row_stages_validate.inc
#

if (`SELECT '$state_info' = ''`)
{
  --let $_state_info_condition= IS NULL
  --let $_state_info_condition_show_processlist= LIKE 'NULL'
}

if (`SELECT '$state_info' <> ''`)
{
  --let $_state_info_condition= LIKE "$state_info"
  --let $_state_info_condition_show_processlist= $_state_info_condition
}

if (`SELECT '$state_thd_name' = ''`)
{
  if (`SELECT @@replica_parallel_workers > 0`)
  {
    --let $_stage_thd_name="thread/sql/replica_worker"
  }

  if (`SELECT @@replica_parallel_workers = 0`)
  {
   --let $_stage_thd_name="thread/sql/replica_sql"
  }
}

if (`SELECT '$state_thd_name' <> ''`)
{
  --let $_stage_thd_name="$state_thd_name"
}

--let $assert_text= PERFORMANCE_SCHEMA.threads reports: state=$state_stage, db=$state_db, info $_state_info_condition, $state_command
--let $assert_cond= COUNT(*)=1 FROM performance_schema.threads WHERE NAME=$_stage_thd_name AND PROCESSLIST_DB="$state_db" AND PROCESSLIST_STATE="$state_stage" AND PROCESSLIST_INFO $_state_info_condition AND PROCESSLIST_COMMAND = "$state_command"
--source include/assert.inc

# validate information_schema (state is set to debug sync point)
--let $assert_text= INFORMATION_SCHEMA.processlist reports: db=$state_db, info $_state_info_condition, command=$state_command
--let $assert_cond= COUNT(*)=1 FROM information_schema.processlist WHERE info $_state_info_condition AND command="$state_command" AND db = "$state_db"
--source include/assert.inc

if (!$state_skip_show_processlist)
{
  --echo [SHOW PROCESSLIST reports: db=$state_db, info $_state_info_condition_show_processlist, command=$state_command]
  # validate processlist
  --let $show_statement= SHOW PROCESSLIST
  --let $field= Info
  --let $condition= $_state_info_condition_show_processlist
  --source include/wait_show_condition.inc

  --let $show_statement= SHOW PROCESSLIST
  --let $field= db
  --let $condition= = "$state_db"
  --source include/wait_show_condition.inc

  --let $show_statement= SHOW PROCESSLIST
  --let $field= Command
  --let $condition= = "$state_command"
  --source include/wait_show_condition.inc
}
