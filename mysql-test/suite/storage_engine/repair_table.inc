# 
# REPAIR TABLE statements
#
# Note: the output is likely to be different for the engine under test,
# in which case rdiff will be needed. Or, the output might say that
# the storage engine does not support REPAIR.
#

--disable_warnings
DROP TABLE IF EXISTS t1,t2;
--enable_warnings

--source create_table.inc
INSERT INTO t1 (a,b) VALUES (1,'a'),(2,'b');
--let $table_name = t2
--source create_table.inc

REPAIR TABLE t1;
if ($mysql_errname)
{
  --source unexpected_result.inc
}
INSERT INTO t1 (a,b) VALUES (3,'c');
INSERT INTO t2 (a,b) VALUES (4,'d');
REPAIR NO_WRITE_TO_BINLOG TABLE t1, t2;
INSERT INTO t2 (a,b) VALUES (5,'e'),(6,'f');
REPAIR LOCAL TABLE t2;
INSERT INTO t1 (a,b) VALUES (7,'g'),(8,'h');
INSERT INTO t2 (a,b) VALUES (9,'i');
REPAIR LOCAL TABLE t2, t1 EXTENDED;
INSERT INTO t1 (a,b) VALUES (10,'j');
INSERT INTO t2 (a,b) VALUES (11,'k');
REPAIR TABLE t1, t2 QUICK USE_FRM;
INSERT INTO t1 (a,b) VALUES (12,'l');
INSERT INTO t2 (a,b) VALUES (13,'m');
REPAIR NO_WRITE_TO_BINLOG TABLE t1, t2 QUICK EXTENDED USE_FRM;
FLUSH TABLE t1;

let my_datadir = `SELECT @@datadir`;

# Now we'll override all table files except for frm.
# Some engines are more enduring to table files corruption
# than others, so the result of the following INSERT and REPAIR
# will be different for different engines

--perl
@files = glob "$ENV{my_datadir}/test/t1.*";
foreach (@files)
{
  next if /.frm$/;
  rename($_,"$_.save");
  open(FILE,">$_") || print "Could not open $_\n" && exit;
  print FILE "";
  close(FILE);
}
EOF

# We don't worry so much about the INSERT or SELECT result,
# it's REPAIR that we are after. 
# The preceding INSERT, however, helps to trigger 
# a bit more internals
--let $error_codes = 0,130,ER_FAILED_READ_FROM_PAR_FILE,ER_OPEN_AS_READONLY
INSERT INTO t1 (a,b) VALUES (14,'n');
--source check_errors.inc
CHECK TABLE t1;
--let $error_codes = 0,130,ER_FAILED_READ_FROM_PAR_FILE,ER_OPEN_AS_READONLY
SELECT a,b FROM t1;
--source check_errors.inc
--enable_warnings
REPAIR TABLE t1;

--perl
@files = glob "$ENV{my_datadir}/test/t1.*.save";
foreach (@files)
{
  $nm = $_;
  $nm =~ s/\.save$//;
  rename($_,$nm);
}
EOF

DROP TABLE t1, t2;

--let $continue = 1
--source have_default_index.inc

if ($have_default_index)
{
  call mtr.add_suppression("Got an error from thread_id=.*");
  call mtr.add_suppression("MySQL thread id .*, query id .* localhost.*root Checking table");
  call mtr.add_suppression(" '\..test.t1'");
  call mtr.add_suppression("Couldn't repair table: test.t1");

  --let $create_definition = a $int_indexed_col, b $char_col, $default_index (a)
  --source create_table.inc
  REPAIR TABLE t1;
  INSERT INTO t1 (a,b) VALUES (7,'g'),(8,'h');
  REPAIR TABLE t1 EXTENDED;
  INSERT INTO t1 (a,b) VALUES (10,'j');
  REPAIR TABLE t1 USE_FRM;

  # We will take files one by one (except for frm file),
  # save the file, update the table, then restore the file
  # and check the table.
  # Results here can be very different depending on the engine.

  let $my_errno = 0;

  --list_files $my_datadir/test

  while (!$my_errno)
  {
    --error 0,2
    --perl 
    use File::Copy;
    @files = glob "$ENV{my_datadir}/test/t1*";
    foreach (@files)
    {
      next if /.(?:frm|save|done)$/;
      next if -e "$_.done";
      copy($_,"$_.save");
      exit 0;
    }
    # No more files
    exit 2;
  EOF
    let $my_errno = $errno;
    if (!$my_errno)
    {
      --let $error_codes = 0,144
      INSERT INTO t1 (a,b) VALUES (14,'n'),(15,'o');
      --source check_errors.inc
      FLUSH TABLE t1;
      --replace_result $my_datadir <DATADIR>
      --perl
      use File::Copy;
      @files = glob "$ENV{my_datadir}/test/t1*.save";
      $nm = $files[0]; 
      $nm =~ s/\.save$//;
      print "Restoring $nm\n";
      copy($files[0],"$nm.done");
      rename($files[0],$nm);
  EOF
      CHECK TABLE t1;
      --let $error_codes = 0,ER_NOT_KEYFILE,144
      SELECT a,b FROM t1;
      --source check_errors.inc
    }
  }
  DROP TABLE t1;
}

--remove_files_wildcard $my_datadir/test t1*


