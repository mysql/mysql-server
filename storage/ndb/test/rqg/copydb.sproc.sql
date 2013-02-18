delimiter |;
CREATE PROCEDURE copydb(dstdb varchar(64), srcdb varchar(64))
BEGIN

  declare tabname varchar(255);
  declare done integer default 0;
  declare c cursor for 
    SELECT table_name
    FROM INFORMATION_SCHEMA.TABLES where table_schema = srcdb;

  declare continue handler for not found set done = 1;

  open c;
  
  repeat
    fetch c into tabname;
    if not done then
       set @ddl = CONCAT('CREATE TABLE ', dstdb, '.', tabname, 
                         ' LIKE ', srcdb, '.', tabname);
       PREPARE stmt from @ddl;
       EXECUTE stmt;

       set @ddl = CONCAT('INSERT INTO ', dstdb, '.', tabname, 
                         ' SELECT * FROM ', srcdb, '.', tabname);
       PREPARE stmt from @ddl;
       EXECUTE stmt;
    end if;
  until done end repeat;
  close c;
END;
\G
