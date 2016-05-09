#
# Bug#17666696: 'CONNECTIONS' SERVER STATUS VARIABLE SHOWS +1 VALUE
#
SELECT CONNECTION_ID() INTO @id1;
SELECT MAX(processlist_id) FROM performance_schema.threads INTO @id2;
SELECT variable_value FROM performance_schema.global_status WHERE variable_name='connections' INTO @id3;
# connection_id() highest in pfs.threads?
SELECT (@id1=@id2);
(@id1=@id2)
1
# same as in global_status?
SELECT (@id2=@id3);
(@id2=@id3)
1
# same as in processlist?
SELECT (@id3=@id4);
(@id3=@id4)
1
