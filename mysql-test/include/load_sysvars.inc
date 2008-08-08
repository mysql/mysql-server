#
# This script is included by tests of system variables to include system
# dependant values for the variables: default, minimum and maximum values
#

if (`SELECT convert(@@version_compile_os using latin1)
            IN ('Win32','Win64','Windows')`)
{
   --source include/windows_sys_vars.inc
}
if (!`SELECT convert(@@version_compile_os using latin1)
             IN ('Win32','Win64','Windows')`)
{
   --source include/linux_sys_vars.inc
}
