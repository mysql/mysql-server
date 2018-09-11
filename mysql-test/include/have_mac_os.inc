if (`select convert(@@version_compile_os using latin1) RLIKE '^(osx|macos)' = 0`)
{
  skip Needs OSX or MacOS;
}
