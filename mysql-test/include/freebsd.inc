if (`select convert(@@version_compile_os using latin1) LIKE ("%FreeBSD%") = 0`)
{
  skip Needs FreeBSD;
}

