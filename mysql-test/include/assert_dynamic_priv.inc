# ==== Purpose ====
#
# Check if a dynamic privilege is registered or not.
#
# ==== Usage ====
#
# --let $assert_privilege_name= AUDIT_ABORT_EXEMT
# --let $assert_privilege_absent= ON
# --source include/assert_dynamic_priv.inc
#
# Parameters:
#
# $assert_privilege_name
#   The global privilege's name

# $assert_privilege_absent
#   if ON it will assert if the privilege is absent, otherwise if the privilege is present
#

let $assert_file=$MYSQLTEST_VARDIR/tmp/assert_dynamic_priv.out;
--let $assert_select=$assert_privilege_name
let $assert_text= "$assert_privilege_name should be defined";
let $assert_count=1;

if ($assert_privilege_absent == "ON")
{
  let $assert_text= "$assert_privilege_name should not be defined";
  let $assert_count=0;
}

output $assert_file;
SHOW PRIVILEGES;

source include/assert_grep.inc;
remove_file $assert_file;
