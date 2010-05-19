# Use replace_regex to remove non-deterministic values from the
# ndb_desc output
#
--replace_regex /Version: [0-9]*/Version: Any/  /t2_unique_index\(j\) - OrderedIndex/Index/ /t2_unique_index\$unique\(j\) - UniqueHashIndex/Index/ /PRIMARY\(i\) - OrderedIndex/Index/  /BT=NDB\$BLOB_[0-9]*_/BT=NDB$BLOB_XX_/  /Length of frm data: [0-9]*/Length of frm data: XXX/ 
--exec $NDB_DESC --no-defaults $ndb_desc_opts
