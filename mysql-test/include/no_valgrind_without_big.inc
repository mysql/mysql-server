# If we are running with Valgrind ($VALGRIND_TEST <> 0) than the resource
# consumption (storage space needed, runtime ...) will be extreme.
# Therefore we require that the option "--big-test" is also set.

if ($VALGRIND_TEST)
{
  if (!$BIG_TEST)
  {
    --skip Need '--big-test' or '--only-big-test' when running with Valgrind
  }
}
