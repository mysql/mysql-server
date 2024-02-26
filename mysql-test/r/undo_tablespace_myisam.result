#
# MyISAM does not support undo tablespaces commands
#
CREATE UNDO TABLESPACE undo_003 ADD DATAFILE 'undo_003.ibu' ENGINE MyISAM;
ERROR HY000: Table storage engine 'MyISAM' does not support the create option 'CREATE UNDO TABLESPACE'
ALTER UNDO TABLESPACE undo_003 SET ACTIVE ENGINE MyISAM;
ERROR HY000: Table storage engine 'MyISAM' does not support the create option 'ALTER UNDO TABLESPACE'
ALTER UNDO TABLESPACE undo_003 SET INACTIVE ENGINE MyISAM;
ERROR HY000: Table storage engine 'MyISAM' does not support the create option 'ALTER UNDO TABLESPACE'
DROP UNDO TABLESPACE undo_003 ENGINE MyISAM;
ERROR HY000: Table storage engine 'MyISAM' does not support the create option 'DROP UNDO TABLESPACE'
