
# ==== Purpose ====
#
# Extracts the value of privilege_checks_user account for a replication
# server instance from the input parameter $rpl_privilege_checks_user. It sets
# the variable:
#
#   $rpl_instance_privilege_checks_user
#     Privilege_checks_user account for the replication server specified in
#     input parameter $rpl_server_number
#
#
# ==== Usage ====
#
# [--let $rpl_privilege_checks_user= *:'user'@'host',1:'user'@'host',2:NULL]
# [--let $rpl_server_number= 1]
#  -- source include/rpl/get_instance_priv_checks_user.inc
#
# Parameters:
#
#   $rpl_privilege_checks_user
#     Definition of privilege_checks_user account for all servers.
#     See include/rpl/change_topology.inc
#
#   $rpl_server_number
#     The server number of the slave
#
#

--let $include_filename= rpl/get_instance_priv_checks_user.inc
--source include/begin_include_file.inc

if ($rpl_privilege_checks_user == ''){
--die ERROR IN TEST: you must set \$rpl_privilege_checks_user before sourcing rpl/get_instance_priv_checks_user.inc
}

# Get PRIVILEGE_CHECKS_USER for current `$rpl_server_number`
--let $_rpl_change_topology_helper_privilege_checks_index= `SELECT LOCATE('$rpl_server_number:', "$rpl_privilege_checks_user")`

if ($_rpl_change_topology_helper_privilege_checks_index)
{
--let $rpl_instance_privilege_checks_user= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_privilege_checks_user", $_rpl_change_topology_helper_privilege_checks_index), ',', 1), ':', -1)`
}

# Get PRIVILEGE_CHECKS_USER for all replication servers
--let $_rpl_change_topology_privilege_checks_all_index= `SELECT LOCATE('*:', "$rpl_privilege_checks_user")`
if ($_rpl_change_topology_privilege_checks_all_index)
{
  --let $_rpl_change_topology_privilege_checks_all= `SELECT SUBSTRING_INDEX(SUBSTRING_INDEX(SUBSTRING("$rpl_privilege_checks_user", $_rpl_change_topology_privilege_checks_all_index), ',', 1), ':', -1)`
}

if (`SELECT $_rpl_change_topology_helper_privilege_checks_index = 0 AND "$_rpl_change_topology_privilege_checks_all" != ""`)
{
--let $rpl_instance_privilege_checks_user= $_rpl_change_topology_privilege_checks_all
}

--let $include_filename= rpl/get_instance_priv_checks_user.inc
--source include/end_include_file.inc
