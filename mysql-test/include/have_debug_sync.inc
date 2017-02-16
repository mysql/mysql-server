if (`select count(*) = 0 from information_schema.session_variables where variable_name = 'debug_sync' and variable_value like 'on %'`)
{
  skip debug_sync is not available;
}
