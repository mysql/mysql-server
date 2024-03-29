
echo # Checking if requested schema was selected.;

if ($expect_error != '')
{
  --replace_result $expect_error EXPECTED_ERROR_CODE
  --error 1
}
exec $execute_command
  --user=$user
  --host=127.0.0.1
  -e "SELECT IF(DATABASE()='$schema','Success','Failure');"
  $schema 2>&1;

--let $expect_error=
--let $user=
--let $schema=
--let $execute_command=
--let $expect_error=
