# ==== Purpose ====
#
# Shut down mysqlrouter, and then start it again.
#
# Note: this waits for the current connection to reconnect to router,
# but does not wait for other connections. If your test uses multiple
# connections, you must also use include/wait_until_disconnected.inc
# on all other connections.
#
# ==== Usage ====
#
# --let $keyring_operation=(init|set)
# --let $keyring_set_path_and_value=[ACCOUNT PROPERTY VALUE[,ACCOUNT PROPERTY VALUE]*]
# [--let $keyring_file=FILE_NAME]
# [--let $keyring_master_file=FILE_NAME]
# --source ../include/mysqlrouter_keyring.inc
#
# Parameters:
#
#   $keyring_operation
#     init - create new keyring and "set",
#     set  - fill the keyring with values specifiled in $keyring_set_path_and_value.
#
#   $keyring_set_path_and_value
#     Set property and value for given account (format: ACCOUNT PROPERTY VALUE).
#
#   $keyring_file
#     optional parameter. The script by default uses MySQL Router specific path.
#     Specifies location of the file that stores ACCOUNT/PROPERTY/VALUE entries.
#
#   $keyring_master_file
#     optional parameter. The script by default uses MySQL Router specific path.
#     Specifies location of the file that stores encryption key for $keyring_file.
#

if (!$keyring_operation)
{
  --die Keyring operation is required, please set it to one of [set, init, get, list].
}

--let $keyring_init=
if ($keyring_operation != "init")
{
  if (!$keyring_set_path_and_value)
  {
    --die Keyring require path/value list to be set.
  }
}

if (!$keyring_file)
{
  --mkdir $MYSQL_TMP_DIR/router.$router_id/data/
  --let $keyring_file=$MYSQL_TMP_DIR/router.$router_id/data/keyring
}

if (!$keyring_master_file)
{
  --let $keyring_master_file=$MYSQL_TMP_DIR/router.$router_id/mysqlrouter.key
}

let $paths=$keyring_set_path_and_value;

if (!$do_not_echo_parameters)
{
  --echo # mysqlrouter_keyring
}

if ($keyring_operation == "init")
{
    exec $MYSQLROUTER_KEYRING
      init
      $keyring_file
      --master-key-file $keyring_master_file;
}


while($paths)
{
  if ($keyring_operation == "init")
  {
    --let $keyring_operation="set"
  }

  #
  # split $paths variable using "," as delimiter
  #
  let $paths_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX('$paths', ",", 1),"\n",""))`;
  let $paths=`SELECT TRIM(SUBSTRING('$paths', LENGTH('$paths_element') + 2))`;

  if (!$do_not_echo_parameters)
  {
    --echo #  * $keyring_operation $paths_element
  }

  exec $MYSQLROUTER_KEYRING
      $keyring_operation
      $keyring_file
      --master-key-file $keyring_master_file
      $paths_element;
}

--let $keyring_operation=
--let $keyring_set_path_and_value=
