## Script wait_for_status_variables.inc
#
# $wait_for_status_variable -
#    Name of status variables on which application should
#    wait. Multiple variables are separated by comma.
#
# $wait_for_status_value -
#    Value of status variable. Application must wait
#    until the status variable gets this value.
#
# $wait_quiet -
#    If this variable is set to "1", then its going to
#    disable the output of this script.
#
## Usage
#
# --let $wait_quiet = 0[,1]
# --let $wait_for_status_value = UNSIGNED INT[,NEXT[,NEXT...]]
# --let $wait_for_status_variable = 'STATUS VARIABLE NAME'[,NEXT[,NEXT...]]
# --source ../include/wait_for_status_variables.inc
#

if($wait_quiet=="")
{
    let $wait_quiet=0;
}

if (!$wait_for_status_variable)
{
  die wait_for_status_variables.inc requires 'wait_for_status_variable' variable to be set;
}

if (`SELECT LENGTH("$wait_for_status_value") = 0`)
{
  if ($wait_for_status_value != "0")
  {
    die wait_for_status_variables.inc requires 'wait_for_status_value' variable to be set;
  }
}

let $status=$wait_for_status_variable;
let $value=$wait_for_status_value;

while($status)
{
  #
  # split status variable names using "," as delimiter
  #
  let $status_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$status", ",", 1),"\n",""))`;
  let $status=`SELECT TRIM(SUBSTRING("$status", LENGTH("$status_element") + 2))`;

  #
  # split status variable values using "," as delimiter
  #
  let $value_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$value", ",", 1),"\n",""))`;
  let $value=`SELECT TRIM(SUBSTRING("$value", LENGTH("$value_element") + 2))`;
  if ($wait_quiet!=1)
  {
    echo # Syncing with status [ $status_element == $value_element ];
  }

  let $wait_condition=
    SELECT VARIABLE_VALUE = $value_element
      FROM  performance_schema.global_status
      WHERE VARIABLE_NAME = $status_element;
  --source include/wait_condition_or_abort.inc
}

--let $wait_for_status_variable=
--let $wait_for_status_value=
--let $wait_quiet=
