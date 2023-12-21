--echo # Should use myisam
--source include/force_myisam_default.inc
--source include/have_myisam.inc

# Bug#31293 - Incorrect parser errors for create/alter/drop logfile group/tablespace
#

-- error ER_ILLEGAL_HA_CREATE_OPTION
create logfile group ndb_lg1 add undofile 'ndb_undo1.dat' initial_size=32M engine=myisam;
-- error ER_ILLEGAL_HA_CREATE_OPTION
create logfile group ndb_lg1 add undofile 'ndb_undo1.dat' engine=myisam;
-- error ER_ILLEGAL_HA_CREATE_OPTION
create logfile group ndb_lg1 add undofile 'ndb_undo1.dat';

-- error ER_ILLEGAL_HA_CREATE_OPTION
create tablespace ndb_ts1 add datafile 'ndb_ts1.dat' use logfile group ndb_lg1 engine=myisam initial_size=32M;
-- error ER_ILLEGAL_HA_CREATE_OPTION
create tablespace ndb_ts1 add datafile 'ndb_ts1.dat' use logfile group ndb_lg1 engine=myisam;
-- error ER_ILLEGAL_HA_CREATE_OPTION
create tablespace ndb_ts1 add datafile 'ndb_ts1.dat' use logfile group ndb_lg1 engine=myisam;

-- error ER_ILLEGAL_HA_CREATE_OPTION
alter logfile group ndb_lg1 add undofile 'ndb_undo1.dat' wait engine=myisam;
-- error ER_ILLEGAL_HA_CREATE_OPTION
alter logfile group ndb_lg1 add undofile 'ndb_undo1.dat' engine=myisam;
-- error ER_ILLEGAL_HA_CREATE_OPTION
alter logfile group ndb_lg1 add undofile 'ndb_undo1.dat' engine=myisam;

-- error ER_TABLESPACE_MISSING_WITH_NAME
alter tablespace ndb_ts1 add datafile 'ndb_ts1.dat' initial_size=32M engine=myisam;
-- error ER_TABLESPACE_MISSING_WITH_NAME
alter tablespace ndb_ts1 add datafile 'ndb_ts1.dat' engine=myisam;
-- error ER_TABLESPACE_MISSING_WITH_NAME
alter tablespace ndb_ts1 add datafile 'ndb_ts1.dat' engine=myisam;

-- error ER_ILLEGAL_HA_CREATE_OPTION
drop logfile group ndb_lg1 engine=myisam;
-- error ER_ILLEGAL_HA_CREATE_OPTION
drop logfile group ndb_lg1 engine=myisam;

-- error ER_TABLESPACE_MISSING_WITH_NAME
drop tablespace ndb_ts1;
-- error ER_TABLESPACE_MISSING_WITH_NAME
drop tablespace ndb_ts1;

