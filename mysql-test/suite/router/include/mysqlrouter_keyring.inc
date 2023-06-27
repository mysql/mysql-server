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

if (!$restart_router_id)
{
  let $restart_router_id = 1;
}

if (!$keyring_file)
{
  --mkdir $MYSQL_TMP_DIR/router.$restart_router_id/data/
  --let $keyring_file=$MYSQL_TMP_DIR/router.$restart_router_id/data/keyring
}

if (!$keyring_master_file)
{
  --let $keyring_master_file=$MYSQL_TMP_DIR/router.$restart_router_id/mysqlrouter.key
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
