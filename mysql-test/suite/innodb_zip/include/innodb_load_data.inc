--echo # Load the data

SET @col_1 = repeat('a', 100);
SET @col_2 = repeat('b', 100);
SET @col_3 = repeat('c', 100);
SET @col_4 = repeat('d', 100);
SET @col_5 = repeat('e', 100);
SET @col_6 = repeat('f', 100);
SET @col_7 = repeat('g', 100);
SET @col_8 = repeat('h', 100);
SET @col_9 = repeat('i', 100);

while ($i) {

eval INSERT INTO tab5
VALUES (@col_1,@col_2,@col_3,@col_4,@cl_5,@col_6,@col_7,@col_8,@col_9);
dec $i;
}
commit;
