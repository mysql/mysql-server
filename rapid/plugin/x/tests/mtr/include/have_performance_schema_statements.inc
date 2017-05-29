# Do not run the test when PSI statement monitoring is disabled
if (`SELECT COUNT(*)=0 AS statement FROM performance_schema.events_statements_current;`)
{
  skip Needs DISABLE_PSI_STATEMENT (cmake options) to be set to zero;
}
