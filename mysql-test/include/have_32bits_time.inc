# see also have_64bits_time.inc

let $have_64bit = `SELECT @@version_compile_machine IN ('x86_64', 'amd64', 'sparc')`;
if ($have_64bit) {
  --skip Doesn't support 32 bits UNIX time, only 64 bits
}

let $have_64bits_time = 0;
