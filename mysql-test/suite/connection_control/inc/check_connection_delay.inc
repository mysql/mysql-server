# Following variables should be set:
# $USER                 Name of the user
# $PASSWORD             Password to be supplied
# $SUCCESS              Whether a successful connection is expected or not
# $DELAY_STATS           Expected value of Connection_control_delay_generated
# $USE_AUTH_PLUGIN      Whether an authentication plugin is to be used or not
# $CLIENT_AUTH_PLUGIN   Authentication plugin

connection default;
disable_query_log;
disable_result_log;

if ($SUCCESS == 0)
{
  --echo # Connection attempt should fail.
  if ($USE_AUTH_PLUGIN == 0)
  {
    --error 1
    --exec $MYSQL -u$USER -p$PASSWORD -e "SELECT 1;" 2>&1
  }
  if ($USE_AUTH_PLUGIN == 1)
  {
    --error 1
    --exec $MYSQL -u$USER $CLIENT_AUTH_PLUGIN -p$PASSWORD -e "SELECT 1;" 2>&1
  }
}

if ($SUCCESS != 0)
{
  --echo # Connection attempt should succeed.
    if ($USE_AUTH_PLUGIN == 0)
  {
    --exec $MYSQL -u$USER -p$PASSWORD -e "SELECT 1;" 2>&1
  }
  if ($USE_AUTH_PLUGIN == 1)
  {
    --exec $MYSQL -u$USER $CLIENT_AUTH_PLUGIN -p$PASSWORD -e "SELECT 1;" 2>&1
  }
}

enable_result_log;
--echo Connection_control_delay_generated should be $DELAY_STATS
SHOW STATUS LIKE 'Connection_control_delay_generated';
enable_query_log;
