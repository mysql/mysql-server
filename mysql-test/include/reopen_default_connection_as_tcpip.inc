# When running on Windows the default connection will be a named pipe
# connection. A named pipe connection is not managed by the thread pool in the
# normal way, so it may be necessary to close it and replace it with a tcp-ip
# connection.

--source include/check_windows.inc

if ($have_windows)
{
  disconnect default;
  --source include/wait_until_disconnected.inc
  connect (default, localhost, root,,);
}

