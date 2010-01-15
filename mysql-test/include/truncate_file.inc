# truncate a giving file, all contents of the file are be cleared

if (`SELECT 'x$file' = 'x'`)
{
  --echo Please assign a file name to $file!!
  exit;
}

let TRUNCATE_FILE= $file;

perl;
use Env;
Env::import('TRUNCATE_FILE');
open FILE, '>', $TRUNCATE_FILE || die "Can not open file $file";
close FILE;
EOF
