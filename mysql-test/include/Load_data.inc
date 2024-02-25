--echo ===============
--echo Load the data
--echo ===============

SET @col_1 = repeat('a', 10);

while ($i) {

eval INSERT INTO $table
VALUES (@col_1);
dec $i;
}
commit;

