--echo -------------------------------------------------------------------------
--echo ---  Delete rows and partitions of tables with $sqlfunc
--echo -------------------------------------------------------------------------

eval delete from $t1 where col1=$val2;
eval delete from $t2 where col1=$val2;
eval delete from $t3 where col1=$val2;
eval delete from $t4 where col1=$val2;
eval delete from $t5 where col1=$val2;
eval delete from $t6 where col1=$val2;

eval select * from $t1;
eval select * from $t2;
eval select * from $t3;
eval select * from $t4;
eval select * from $t5;

eval insert into $t1 values ($val2);
eval insert into $t2 values ($val2);
eval insert into $t3 values ($val2);
eval insert into $t4 values (4,$val2);
eval insert into $t5 values (4,$val2);
eval insert into $t6 values (4,$val2);

eval select * from $t1;
eval select * from $t2;
eval select * from $t3;
eval select * from $t4;
eval select * from $t5;
eval select * from $t6;

eval alter table $t1 drop partition p0; 
eval alter table $t2 drop partition p0; 
eval alter table $t4 drop partition p0; 
eval alter table $t5 drop partition p0; 
eval alter table $t6 drop partition p0;  

eval select * from $t1;
eval select * from $t2;
eval select * from $t3;
eval select * from $t4;
eval select * from $t5;
eval select * from $t6;
