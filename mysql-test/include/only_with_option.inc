# ==== Purpose ====
#
# Skip the test when the comparision between a given variable and variable
# value, using a given comparision operator, evaluates to false.
#
# ==== Usage ====
#
# --let $option_name = <OPTION>
# --let $option_value = <VALUE>
# [--let $option_operator = <ANY SQL COMPARISION OPERATOR>, defaults to `=`]
# [--let $option_scope = GLOBAL|SESSION, defaults to GLOBAL]
# --source include/only_with_option.inc
#
# Parameters:
#
#   $option_name
#     The name of the option to be tested.
#
#   $option_value
#     The value we want the option tested against.
#
#   $option_operator
#     The comparision operator to test the option against value. Defaults
#     to `=`.
#
#   $option_scope
#     The scope of the option, either SESSION or GLOBAL. Default to GLOBAL.
#

if ($option_name == '')
{
  --die ERRROR: `option_name` parameter must be set
}
if ($option_value == '')
{
  --die ERRROR: `option_value` parameter must be set
}

--let $_scope = GLOBAL
--let $_operator = =

if ($option_scope != '')
{
    --let $_scope = $option_scope
}
if ($option_operator != '')
{
    --let $_operator = $option_operator
}

--let $_option_name = $_scope.$option_name

if (`SELECT not(@@$_option_name $_operator $option_value)`)
{
  --skip Test only executed when `$_option_name $_operator $option_value`
}
--echo include/only_with_option.inc [$_option_name $_operator $option_value]

--let $option_name =
--let $option_value =
--let $option_scope =
--let $option_operator =
