if (`select convert(@@version_compile_os using latin1) RLIKE '^(osx|macos)' = 1`)
{
  skip Test requires 'not_mac_os';
}
