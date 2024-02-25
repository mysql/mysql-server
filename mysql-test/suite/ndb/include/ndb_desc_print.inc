# When developing new tests, prefer using ndb_describe_table.inc
# or running succinct queries from ndbinfo tables to including this file

# Check ndb_desc_opts argument variable
if (!$ndb_desc_opts)
{
  echo The variable ndb_desc_opts must be set when calling ndb_desc_print.inc;
  die Missing argument ndb_desc_opts;
}

let $ndb_desc_cmd= $NDB_DESC $ndb_desc_opts;

# Uncomment  the following line in order to run ndb_desc through valgrind
#let $ndb_desc_cmd= valgrind -q $ndb_desc_cmd;

# Use replace_regex to remove non-deterministic values from the
# ndb_desc output
#
replace_regex /Version: [0-9]*/Version: Any/
              /NDB\$BLOB_[0-9]*_/NDB$BLOB_XX_/
              /Length of frm data: [0-9]*/Length of frm data: XXX/
              /IndexTable [0-9]*/IndexTable XX/
              /^([0-9][0-9]*) [0-9 	]*/\1 .../
              /Table id: [0-9]*/Table id: XXX/
              /Tablespace id: [0-9]*/Tablespace id: XX/;
--exec $ndb_desc_cmd

# Reset argument variable in order to detect missing assignment
let $ndb_desc_opts=;
