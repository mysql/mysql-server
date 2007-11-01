/* This file is needed for building a dynamic InnoDB plugin that
can replace the builtin InnoDB plugin in MySQL.
It must be generated as follows:

* compile the InnoDB plugin
* overwrite include/innodb_redefine.h with the following command
* compile the final InnoDB plugin

nm .libs/ha_innodb.so.0.0.0|
sed -ne 's/^[^ ]* . \([a-zA-Z][a-zA-Z0-9_]*\)$/#define \1 ibd_\1/p'|
grep -v 'innodb_hton_ptr\|builtin_innobase_plugin' > include/innodb_redefine.h
*/
