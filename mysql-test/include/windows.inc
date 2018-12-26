if (`select convert(@@version_compile_os using latin1) IN ("Win32","Win64","Windows") = 0`)
{
  skip Need windows;
}

