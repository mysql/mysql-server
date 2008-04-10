#
# This script is included by every test to include system dependant variables'
# default, minimum and maximum values
#


if (`select convert(@@version_compile_os using latin1) IN ("Win32","Win64","Windows")`)
{
   --source include/windows_sys_vars.inc
}
if (`select convert(@@version_compile_os using latin1) IN ("suse-linux-gnu","redhat-linux-gnu","pc-solaris2.10")`)
{
   --source include/linux_sys_vars.inc
}
