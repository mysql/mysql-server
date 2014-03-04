if (`select convert(@@version_compile_os using latin1) IN ("Linux") = 0`)
{
  skip Needs Linux;
}

