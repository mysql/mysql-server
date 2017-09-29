if (`select convert(@@version_compile_os using latin1) LIKE ("%osx%") = 0`)
{
  skip Needs MacOSX;
}
