# see also have_32bits_time.inc

let $have_32bit = `SELECT @@version_compile_machine = 'i686'`;

if ($have_32bit) {
  --skip Doesn't support 64 bits UNIX time
}

let $have_64bits_time = 1;




