#
# Set desired charset_connection and collation_collation
# before including this file.
#

# The next query creates a LONGTEXT column
# using the current character_set_connection
# and collation_connection.

create table t1 select repeat('a',4000) a;
delete from t1;

insert into t1 values ('a'), ('a '), ('a\t');
select collation(a),hex(a) from t1 order by a;
drop table t1;
