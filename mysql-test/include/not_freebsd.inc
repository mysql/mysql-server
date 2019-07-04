if (`select convert(@@version_compile_os using latin1) LIKE ("%FreeBSD%") = 1`)
{
  skip Test requires 'not_freebsd';
}

