# Maria helper script
# Copies table' data and index file to other directory, or back, or compares.
# The other directory looks like a database directory, so that we can
# read copies from inside mysqld, that's also why we copy the frm.

# "mms" is a namespace for Maria_Make_Snapshot

# API:
# 1) set one of
# $mms_copy : to copy table from database to spare directory
# $mms_reverse : to copy it back
# $mms_compare_physically : to compare both byte-for-byte
# 2) set $mms_tname to a string and set $mms_table_to_use to a number: tables
# will be mysqltest.$mms_tname$mms_table_to_use.
# 3) set $mms_purpose to say what this copy is for (influences the naming
# of the spare directory).

if ($mms_copy)
{
  --echo * copied $mms_tname$mms_table_to_use for $mms_purpose
  copy_file $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAD $MYSQLD_DATADIR/mysqltest_for_$mms_purpose/$mms_tname$mms_table_to_use.MAD;
  copy_file $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAI $MYSQLD_DATADIR/mysqltest_for_$mms_purpose/$mms_tname$mms_table_to_use.MAI;
  copy_file $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.frm $MYSQLD_DATADIR/mysqltest_for_$mms_purpose/$mms_tname$mms_table_to_use.frm;
}

if ($mms_reverse_copy)
{
  # do not call this without flushing target table first!
  --disable_warnings
  --echo * copied $mms_tname$mms_table_to_use back for $mms_purpose
  -- error 0,1
  remove_file $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAD;
  --enable_warnings
  copy_file $MYSQLD_DATADIR/mysqltest_for_$mms_purpose/$mms_tname$mms_table_to_use.MAD $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAD;
  --disable_warnings
  -- error 0,1
  remove_file $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAI;
  --enable_warnings
  copy_file $MYSQLD_DATADIR/mysqltest_for_$mms_purpose/$mms_tname$mms_table_to_use.MAI $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAI;
}

if ($mms_compare_physically)
{
  # After the UNDO phase this is normally impossible
  # (UNDO execution has created new log records => pages have new LSNs).
  # So, do this only when testing REDO phase.
  # If UNDO phase, we nevertheless compare checksums
  # (see maria_verify_recovery.inc).
  --echo * compared $mms_tname$mms_table_to_use to old version
  diff_files $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAD $MYSQLD_DATADIR/mysqltest_for_$mms_purpose/$mms_tname$mms_table_to_use.MAD;
# index file not yet recovered
#  diff_files $MYSQLD_DATADIR/mysqltest/$mms_tname$mms_table_to_use.MAI $MYSQLD_DATADIR/mysqltest_for_$mms_purpose/$mms_tname$mms_table_to_use.MAI;
}
