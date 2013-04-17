# Check ndb_desc_opts argument variable
if (!$ndb_desc_opts)
{
  echo The variable ndb_desc_opts must be set when calling ndb_desc_print.inc;
  die Missing argument ndb_desc_opts;
}

let $ndb_desc_cmd= $NDB_DESC --no-defaults $ndb_desc_opts;

# Uncomment  the following line in order to run ndb_desc through valgrind
#let $ndb_desc_cmd= valgrind -q $ndb_desc_cmd;

# Use replace_regex to remove non-deterministic values from the
# ndb_desc output
#
--replace_regex /Version: [0-9]*/Version: Any/  /t2_unique_index\(j\) - OrderedIndex/Index/ /t2_unique_index\$unique\(j\) - UniqueHashIndex/Index/ /PRIMARY\(i\) - OrderedIndex/Index/  /BT=NDB\$BLOB_[0-9]*_/BT=NDB$BLOB_XX_/  /Length of frm data: [0-9]*/Length of frm data: XXX/ 
--exec $ndb_desc_cmd

# Reset argument variable in order to detect missing assignment
let $ndb_desc_opts=;
