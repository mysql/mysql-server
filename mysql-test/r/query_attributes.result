#
# WL#12542: query attributes
#
INSTALL COMPONENT 'file://component_query_attributes';
##### test the mysqltest command
# Should return b
SELECT mysql_query_attribute_string('a');
mysql_query_attribute_string('a')
b
# Should return NULL
SELECT mysql_query_attribute_string('a');
mysql_query_attribute_string('a')
NULL
# Should return b
SELECT mysql_query_attribute_string('a');
mysql_query_attribute_string('a')
b
# Should return b
SELECT mysql_query_attribute_string('a'), mysql_query_attribute_string('c');
mysql_query_attribute_string('a')	mysql_query_attribute_string('c')
b	d
# Should return b c
SELECT mysql_query_attribute_string('a');
mysql_query_attribute_string('a')
b c
SELECT mysql_query_attribute_string('');
mysql_query_attribute_string('')
NULL
# Should return c
SELECT mysql_query_attribute_string('a');
mysql_query_attribute_string('a')
c
# Should return NULL, d
SELECT mysql_query_attribute_string('a'),  mysql_query_attribute_string('c');
mysql_query_attribute_string('a')	mysql_query_attribute_string('c')
NULL	d
##### test the mysql command
# Should return b
mysql_query_attribute_string('a')
b
# Should return c
mysql_query_attribute_string('a')
c
# Should return error
# Should return b d
mysql_query_attribute_string('a')	mysql_query_attribute_string('c')
b	d
# Should return b c
mysql_query_attribute_string('a')
b c
# Should return null
mysql_query_attribute_string('a')
NULL
# Should return null
SELECT mysql_query_attribute_string('a');
mysql_query_attribute_string('a')
NULL
# cleanup
UNINSTALL COMPONENT 'file://component_query_attributes';
# End of 8.0 tests
