# ==== Purpose ====
#
# Skip the test if the utility 'sed' is not found.
# If 'sed' is found, set the variable $SED to the name of the program.

--disable_result_log
--disable_query_log

--let $SED=
--let $_have_sed_outfile= $MYSQLTEST_VARDIR/tmp/have_sed.tmp

# Find sed utility.
--error 0,1,2,127
--exec sed --help > $_have_sed_outfile 2>&1

if (`SELECT LOCATE('Usage', LOAD_FILE('$_have_sed_outfile')) > 0`) {
  --let $SED= sed
}
# sed on solaris might not have "--help" option. It will fail with error 2
# stating: "sed: illegal option -- help"
if (`SELECT LOCATE('illegal option', LOAD_FILE('$_have_sed_outfile')) > 0`) {
  --let $SED= sed
}
--remove_file $_have_sed_outfile
--enable_query_log
--enable_result_log
# Skip the test case if sed not found
if ($SED == '') {
  --skip utility 'sed' not found
}
