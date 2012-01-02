#==============================================================================
# Set IP address defaults with respect to IPV6 support
#
# This file determines the level of support for IPV4, IPV4 mapped or IPV6, then
# sets the appropriate localhost IP format to use for 'connect()' commands.
#
# Input:  $my_socket_debug - Print results of IP version check (optional)
# Output: $my_localhost    - Default localhost IP
#==============================================================================

let $check_ipv6_just_check= 1;
#--source include/check_ipv6.inc

#==============================================================================
# Determine if IPV6 supported
#
# Parameters:
#  $check_ipv6_just_check - Don't skip the test if IPv6 is unsupported,
#                           just set the variable $check_ipv6_supported
#==============================================================================
--disable_query_log
--disable_result_log
--disable_abort_on_error

let $check_ipv6_supported= 1;

connect (checkcon123456789,::1,root,,test);

if($mysql_errno)
{
  let $check_ipv6_supported=0;
  if(!$check_ipv6_just_check)
  {
    skip No IPv6 support;
  }
}

if(!$mysql_errno)
{
  disconnect checkcon123456789;
  --source include/wait_until_disconnected.inc
}

connection default;

--enable_abort_on_error
--enable_result_log
--enable_query_log

#==============================================================================
#
# Determine if IPV4 mapped to IPV6 supported
#
let $check_ipv4_mapped_just_check= 1;
#--source include/check_ipv4_mapped.inc
#==============================================================================
# Check if ipv4 mapped to ipv6 is available.
#
# Parameters:
#  $check_ipv4_mapped_just_check - Don't skip the test if IPv4 mapped is unsupported,
#                                  just set the variable $check_ipv4_mapped_supported
#==============================================================================
--disable_query_log
--disable_result_log
--disable_abort_on_error

let $check_ipv4_mapped_supported= 1;

connect (checkcon123456789a,::FFFF:127.0.0.1,root,,test);

if($mysql_errno)
{
  let $check_ipv4_mapped_supported=0;
  if(!$check_ipv4_mapped_just_check)
  {
    skip No mapped IPv4 support;
  }
}

if(!$mysql_errno)
{
  disconnect checkcon123456789a;
  --source include/wait_until_disconnected.inc
}

connection default;

--enable_abort_on_error
--enable_result_log
--enable_query_log

#==============================================================================
# Set the localhost IP default to use when establishing connections
#
#==============================================================================
let $my_localhost=127.0.0.1;

if($check_ipv6_supported)
{
  let $my_localhost=::1;
}

if($check_ipv4_mapped_supported)
{
  let $my_localhost=::ffff:127.0.0.1;
}

if($my_socket_debug)
{
  --echo IPV6=$check_ipv6_supported, IPV4_MAPPED=$check_ipv4_mapped_supported, LOCALHOST=$my_localhost
}
#==============================================================================

