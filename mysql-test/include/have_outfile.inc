let $outfile= `SELECT LOAD_FILE(CONCAT(@tmpdir, '/outfile.test')) LIKE 'Outfile OK%'`;
remove_file $MYSQLTEST_VARDIR/tmp/outfile.test;

if (!$outfile)
{
  --skip Test requires 'have_outfile'
}
