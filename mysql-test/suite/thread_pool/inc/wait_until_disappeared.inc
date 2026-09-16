# Auxiliary script
#
# Purpose: Wait till all connections with id in ($id_list)
#          disappeared from information_schema.processlist
#          or abort if $wait_timeout seconds have passed.
#
#          $id_list has to be set before calling the current script.
#
# Typical use case:
#          --connection con1
#          let $id_list= `SELECT CONNECTION_ID();
#          ...
#          --connection con2
#          let $id= `SELECT CONNECTION_ID();
#          let $id_list= $id_list , $id;
#          ...
#          --connection default
#          --replace_result $id <kill_id>
#          eval KILL $id;
#          ...
#          --disconnect con2
#          --source ../inc/wait_until_disappeared.inc
#
# Note: Using this routine to ensure that disconnects are finished is
#       significant more straight forward and reliable than using the common sequence
#          --connection con1
#          --disconnect con1
#          --source include/wait_until_disconnected.inc
#

let $wait_condition= SELECT COUNT(*) = 0 FROM information_schema.processlist
WHERE id IN ($id_list);
--source include/wait_condition.inc
if(!$success)
{
   --echo # ERROR The sessions with id IN ($id_list) did not disappear from processlist
   eval SELECT * FROM information_schema.processlist WHERE id IN ($id_list);
   --sleep 30
   eval SELECT * FROM information_schema.processlist WHERE id IN ($id_list);
   --echo # abort
   exit;
}

