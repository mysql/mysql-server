# Remove ibtmp* which are re-generated after each mysqld invocation
# skip auto generated auto.cnf from list_files
--remove_files_wildcard $bugdir ibtmp*
--remove_files_wildcard $bugdir auto.cnf
--list_files $bugdir
--remove_files_wildcard $bugdir ibdata*
--remove_files_wildcard $bugdir ib_logfile*
--remove_files_wildcard $bugdir undo00*
--copy_file $bugdir/bak_ibdata1 $bugdir/ibdata1
--copy_file $bugdir/bak_ibdata2 $bugdir/ibdata2
--copy_file $bugdir/bak_ib_logfile0 $bugdir/ib_logfile0
--copy_file $bugdir/bak_ib_logfile1 $bugdir/ib_logfile1
--copy_file $bugdir/bak_ib_logfile2 $bugdir/ib_logfile2
--copy_file $bugdir/bak_undo001 $bugdir/undo001
--copy_file $bugdir/bak_undo002 $bugdir/undo002
--copy_file $bugdir/bak_undo003 $bugdir/undo003
