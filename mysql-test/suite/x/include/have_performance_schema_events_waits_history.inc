# Do not run the test when PFS events_waits_history monitoring is disabled
if (`SELECT COUNT(*)=0 as `events` from performance_schema.events_waits_history;`)
{
  skip Needs DISABLE_PSI_XXX (cmake options) to be set to zero;
}