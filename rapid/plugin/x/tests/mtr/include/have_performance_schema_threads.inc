# Do not run the test when PFS thread monitoring is disabled
if (`SELECT COUNT(*)=0 as `running_threads` from performance_schema.threads ;`)
{
  skip Needs DISABLE_PSI_THREAD (cmake options) to be set to zero;
}
