# ==== Purpose ====
#
# Configures the user account specified in parameter PRIVILEGE_CHECKS_USER
# of the replication channel. The replication applier runs with the privileges
# of this account.
# This file is normally sourced by rpl/impl/change_topology_helper.inc for
# replication use cases or gr_configuration.inc for group replication use
# cases.
# This file creates the user account of PRIVILEGE_CHECKS_USER, sets its
# privileges according to the specified parameters and revokes all privileges
# granted to the test database during installation.
# It sets the following variable:
#
#   $rpl_change_topology_user_created
#     Specifies the user account created for each server in format :
#     server_number1:user1, server_number2:user2 ...
#
# ==== Usage ====
# [--let $rpl_instance_privilege_checks_user= rpl_applier_priv_user'@'localhost]
# [--let $rpl_privilege_checks_user_dont_create_user= 0]
# [--let $rpl_skip_grant_privilege_checks_user_roles= 0]
# [--let $rpl_privilege_checks_use_roles= 0]
# [--let $rpl_privilege_checks_user_grant_all= 0]
# [--let $rpl_privilege_checks_user_grant_option= 0]
# [--let $rpl_privilege_checks_user_additional_grants= CREATE, INSERT]
# [--let $rpl_privilege_checks_user_revoke_grants= DROP]
#  --source include/rpl/priv_checks_user_configuration.inc
#
# Parameters:
#
#  $rpl_server_number
#     Number of the replication server.
#
#  $rpl_instance_privilege_checks_user
#     Name of the PRIVILEGE_CHECKS_USER user account for the replication server
#     $rpl_server_number. If not set there will no privilege checking for the
#     replication applier.
#
#  $skip_configuration_privilege_checks_user
#     If set, do not use environment variables to configure the grants for
#     PRIVILEGE_CHECKS_USER user account
#
#  $skip_defaults_privilege_checks_user
#     If set, do not assign the default values in the configuration of
#     PRIVILEGE_CHECKS_USER user account.
#
#  $rpl_privilege_checks_user_dont_create_user
#    Force to '1' to not create the user provided in
#   `$rpl_instance_privilege_checks_user`. If set to 0, creates it if doesn't
#    exists and, if created, assigns the `REPLICATION_APPLIER` privilege.
#    Default value is '0'.
#
#  $rpl_skip_grant_privilege_checks_user_roles
#    If set to '1' does not modify the privileges of $rpl_instance_privilege_checks_user
#    account. If set to '0' privileges are modified according to parameters
#    $rpl_privilege_checks_user_grant_all, $rpl_privilege_checks_user_grant_option,
#    $rpl_privilege_checks_user_additional_grants and
#    $rpl_privilege_checks_user_revoke_grants. Default value is '0'.
#
#
#  $rpl_privilege_checks_use_roles
#    If set to '1' create a new role 'sql_applier_thread' and grant this role
#    to account $rpl_instance_privilege_checks_user. If set to '0' do nothing.
#    Default value is '0'.
#
#  $rpl_privilege_checks_user_grant_all
#    If set to '1' PRIVILEGE_CHECKS_USER account has all privileges. If set to
#    '0' PRIVILEGE_CHECKS_USER account has privileges given by
#     $rpl_privilege_checks_user_additional_grants. Default value is '1'.
#
#  $rpl_privilege_checks_user_grant_option
#    If set to '1' PRIVILEGE_CHECKS_USER account has the ability to give to
#    other users any of its privileges. It set to '0' this account cannot grant
#    its privileges. Default value is '0'.
#
#   $rpl_privilege_checks_user_additional_grants
#    If $rpl_privilege_checks_user_grant_all is not '1' specifies additional
#    privileges to be granted to the $rpl_instance_privilege_checks_user user.
#
#   $rpl_privilege_checks_user_revoke_grants
#    Specifies the set of privileges to be revoked from
#    $rpl_instance_privilege_checks_user user.
#

--let $include_filename= rpl/priv_checks_user_configuration.inc
--source include/begin_include_file.inc

# If parameters are not instantiated by the calling test script
# try to get their values from environment variables

if ($skip_configuration_privilege_checks_user == '')
{
  if ($rpl_privilege_checks_use_roles == '')
  {
    --let $rpl_privilege_checks_use_roles= $PRIVILEGE_CHECKS_USE_ROLES
  }
  if ($rpl_privilege_checks_user_dont_create_user == '')
  {
    --let $rpl_privilege_checks_user_dont_create_user= $PRIVILEGE_CHECKS_USER_DONT_CREATE_USER
  }
  if ($rpl_skip_grant_privilege_checks_user_roles == '')
  {
    --let $rpl_skip_grant_privilege_checks_user_roles= $SKIP_GRANT_PRIVILEGE_CHECKS_USER_ROLES
  }
  if ($rpl_privilege_checks_user_grant_all == '')
  {
    --let $rpl_privilege_checks_user_grant_all= $PRIVILEGE_CHECKS_USER_GRANT_ALL
  }
  if ($rpl_privilege_checks_user_grant_option == '')
  {
    --let $rpl_privilege_checks_user_grant_option= $PRIVILEGE_CHECKS_USER_GRANT_OPTION
  }
  if ($rpl_privilege_checks_user_additional_grants == '')
  {
    --let $rpl_privilege_checks_user_additional_grants= $PRIVILEGE_CHECKS_USER_ADDITIONAL_GRANTS
  }
  if ($rpl_privilege_checks_user_revoke_grants == '')
  {
    --let $rpl_privilege_checks_user_revoke_grants= $PRIVILEGE_CHECKS_USER_REVOKE_GRANTS
  }
}

if ($rpl_privilege_checks_use_roles == '')
{
  --let $rpl_privilege_checks_use_roles= 0
}
if ($rpl_privilege_checks_user_dont_create_user == '')
{
  --let $rpl_privilege_checks_user_dont_create_user= 0
}
if ($rpl_skip_grant_privilege_checks_user_roles == '')
{
  --let $rpl_skip_grant_privilege_checks_user_roles= 0
}
if ($rpl_privilege_checks_user_grant_all == '')
{
  --let $rpl_privilege_checks_user_grant_all= 1
  if ($skip_defaults_privilege_checks_user != '')
  {
      --let $rpl_privilege_checks_user_grant_all= 0
  }
}
if ($rpl_privilege_checks_user_grant_option == '')
{
  --let $rpl_privilege_checks_user_grant_option= 0
}

--let $_privilege_checks_user_at_index= `SELECT LOCATE('@', "$rpl_instance_privilege_checks_user")`
--let $_privilege_checks_user_name= $rpl_instance_privilege_checks_user
--let $_privilege_checks_user_host= '%'
if ($_privilege_checks_user_at_index != 0)
{
  --let $_privilege_checks_user_name= `SELECT SUBSTRING("$rpl_instance_privilege_checks_user", 1, $_privilege_checks_user_at_index - 1)`
  --let $_privilege_checks_user_host= `SELECT SUBSTRING("$rpl_instance_privilege_checks_user", $_privilege_checks_user_at_index + 1)`
}

# Check if the user should be created if not exists
--let $_privilege_checks_user_create= 1
if ($privilege_checks_user_dont_create_user == 1)
{
  --let $_privilege_checks_user_create= 0
}

if ($_privilege_checks_user_create == 1)
{

  --let $user_exists =`SELECT EXISTS(SELECT 1 FROM mysql.user WHERE user = $_privilege_checks_user_name)`
  if ($user_exists == 0)
  {
    --let $_sql_log_bin_saved= `SELECT @@sql_log_bin`
    SET @@sql_log_bin = 0;

    --eval CREATE USER $rpl_instance_privilege_checks_user
    --let $rpl_change_topology_user_created = $rpl_change_topology_user_created $rpl_server_number:$rpl_instance_privilege_checks_user

    if ($rpl_skip_grant_privilege_checks_user_roles != 1)
    {
        # Revoke all privileges on the test database from the privilege_checks_user account
        # --eval DELETE FROM mysql.db WHERE user = '' and db = 'test'
        UPDATE mysql.db SET
            Select_priv = 'N',
            Insert_priv = 'N',
            Update_priv = 'N',
            Delete_priv = 'N',
            Create_priv = 'N',
            Drop_priv = 'N',
            Grant_priv = 'N',
            References_priv = 'N',
            Index_priv = 'N',
            Alter_priv = 'N',
            Create_tmp_table_priv = 'N',
            Lock_tables_priv = 'N',
            Create_view_priv = 'N',
            Show_view_priv = 'N',
            Create_routine_priv = 'N',
            Alter_routine_priv = 'N',
            Execute_priv = 'N',
            Event_priv = 'N',
            Trigger_priv = 'N'
          WHERE db = 'test' and user = '';
        FLUSH PRIVILEGES;

      if ($rpl_privilege_checks_user_grant_all == 1)
      {
        --let $_privilege_checks_user_grants= ALL
      }

      if ($rpl_privilege_checks_user_grant_all != 1)
      {
        --let $_privilege_checks_user_additional_grants=

        if($rpl_privilege_checks_user_additional_grants != '')
        {
          --let $_privilege_checks_user_additional_grants= ,$rpl_privilege_checks_user_additional_grants
        }

        --let $_privilege_checks_user_grants = REPLICATION_APPLIER,SESSION_VARIABLES_ADMIN$_privilege_checks_user_additional_grants
      }

      --let $_privilege_checks_user_grant_cmd= GRANT $_privilege_checks_user_grants ON *.*

      if ($rpl_privilege_checks_use_roles != 1)
      {
        --let $_privilege_checks_user_grant_user= $rpl_instance_privilege_checks_user
      }

      if ($rpl_privilege_checks_use_roles == 1)
      {
        --let $RPL_PRIV_ROLE = 'sql_applier_thread'
        --eval CREATE ROLE $RPL_PRIV_ROLE

        --let $_privilege_checks_user_grant_user= $RPL_PRIV_ROLE
      }


      --let $_privilege_checks_user_grant_cmd= $_privilege_checks_user_grant_cmd TO $_privilege_checks_user_grant_user

      if ( $rpl_privilege_checks_user_grant_option == 1 )
      {
        --let $_privilege_checks_user_grant_cmd= $_privilege_checks_user_grant_cmd WITH GRANT OPTION
      }

      --eval $_privilege_checks_user_grant_cmd

      --eval GRANT INSERT ON mtr.test_suppressions TO $_privilege_checks_user_grant_user


      if ($rpl_privilege_checks_user_revoke_grants != '')
        {
          --eval REVOKE $rpl_privilege_checks_user_revoke_grants ON *.* FROM $_privilege_checks_user_grant_user
        }

      if ($rpl_privilege_checks_use_roles == 1)
      {
        --eval GRANT $RPL_PRIV_ROLE TO $rpl_instance_privilege_checks_user
        --eval SET DEFAULT ROLE $RPL_PRIV_ROLE TO $rpl_instance_privilege_checks_user
      }
    }

    --eval SET @@sql_log_bin = $_sql_log_bin_saved
  }
}

--let $include_filename= rpl/priv_checks_user_configuration.inc
--source include/end_include_file.inc
