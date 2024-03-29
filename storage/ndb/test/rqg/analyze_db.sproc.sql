delimiter |;
CREATE PROCEDURE analyze_db (db varchar(64))
BEGIN

  declare tabname varchar(255);
  declare done integer default 0;
  declare c cursor for 
  SELECT table_name
  FROM INFORMATION_SCHEMA.TABLES where table_schema = db;
  declare continue handler for not found set done = 1;

  open c;
  
  repeat
    fetch c into tabname;
    if not done then
       set @ddl = CONCAT('ANALYZE TABLE ', db, '.', tabname);
       PREPARE stmt from @ddl;
       EXECUTE stmt;
    end if;
  until done end repeat;
  close c;
END

