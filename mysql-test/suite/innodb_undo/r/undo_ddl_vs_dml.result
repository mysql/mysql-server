CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu';
CREATE UNDO TABLESPACE undo_004 ADD DATAFILE 'undo_004.ibu';
CREATE UNDO TABLESPACE undo_005 ADD DATAFILE 'undo_005.ibu';
CREATE UNDO TABLESPACE undo_006 ADD DATAFILE 'undo_006.ibu';
CREATE DATABASE africa;
USE africa;
CREATE TABLE serengeti (a INT PRIMARY KEY, b TEXT);
CREATE PROCEDURE crash (IN loops INT, IN undo_number INT)
BEGIN
SET @undo_num = undo_number;
SET @before = 5 * (@undo_num - 2);
SET @during =  25;
SET @after = 25 - @before;
SET @all = @before + @during + @after;
PREPARE stmt_ins FROM "INSERT INTO serengeti VALUES (?, ?)";
SET @a = ((@undo_num - 2) * 10000) + 1;
SET @n = 0;
WHILE (@n < @all) DO
SET @b = concat("Insert #", @a);
EXECUTE stmt_ins USING @a, @b;
SET @a = @a + 1;
SET @n = @n + 1;
END WHILE;
PREPARE stmt FROM "UPDATE serengeti SET b = ? WHERE a = ?";
SET @loop = 0;
WHILE (@loop < loops) DO
SET @a = ((@undo_num - 2) * 10000) + 1;
SET @n = 0;
WHILE (@n < @before) DO
SET @b = concat("Update #", @a, ", loop ", @loop, " BEFORE undo_00", @undo_num, " is SET INACTIVE");
EXECUTE stmt USING @b, @a;
SET @a = @a + 1;
SET @n = @n + 1;
END WHILE;
IF (@undo_num = 3) THEN
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
END IF;
IF (@undo_num = 4) THEN
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
END IF;
IF (@undo_num = 5) THEN
ALTER UNDO TABLESPACE undo_005 SET INACTIVE;
END IF;
IF (@undo_num = 6) THEN
ALTER UNDO TABLESPACE undo_006 SET INACTIVE;
END IF;
SET @n = 0;
WHILE (@n < @during) DO
SET @b = concat("Update #", @a, ", loop ", @loop, " WHILE  undo_00", @undo_num, " is SET INACTIVE");
EXECUTE stmt USING @b, @a;
SET @a = @a + 1;
SET @n = @n + 1;
END WHILE;
IF (@undo_num = 3) THEN
ALTER UNDO TABLESPACE undo_003 SET ACTIVE;
END IF;
IF (@undo_num = 4) THEN
ALTER UNDO TABLESPACE undo_004 SET ACTIVE;
END IF;
IF (@undo_num = 5) THEN
ALTER UNDO TABLESPACE undo_005 SET ACTIVE;
END IF;
IF (@undo_num = 6) THEN
ALTER UNDO TABLESPACE undo_006 SET ACTIVE;
END IF;
SET @n = 0;
WHILE (@n < @after) DO
SET @b = concat("Update #", @a, ", loop ", @loop, " AFTER  undo_00", @undo_num, " is SET ACTIVE");
EXECUTE stmt USING @b, @a;
SET @a = @a + 1;
SET @n = @n + 1;
END WHILE;
SET @loop = @loop + 1;
END WHILE;
END|
CALL africa.crash(10, 3);;
CALL africa.crash(10, 4);;
CALL africa.crash(10, 5);;
CALL africa.crash(10, 6);;
SELECT count(*) from serengeti;
count(*)
200
USE test;
DROP DATABASE africa;
ALTER UNDO TABLESPACE undo_003 SET INACTIVE;
DROP UNDO TABLESPACE undo_003;
ALTER UNDO TABLESPACE undo_004 SET INACTIVE;
DROP UNDO TABLESPACE undo_004;
ALTER UNDO TABLESPACE undo_005 SET INACTIVE;
DROP UNDO TABLESPACE undo_005;
ALTER UNDO TABLESPACE undo_006 SET INACTIVE;
DROP UNDO TABLESPACE undo_006;
