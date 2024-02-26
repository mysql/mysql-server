SET @start_global_value = @@global.default_storage_engine;
SELECT @start_global_value;
@start_global_value
MyISAM
SET @start_session_value = @@session.default_storage_engine;
SELECT @start_session_value;
@start_session_value
MyISAM
'#--------------------FN_DYNVARS_005_01-------------------------#'
SET @@global.default_storage_engine = INNODB;
SET @@global.default_storage_engine = DEFAULT;
SELECT @@global.default_storage_engine;
@@global.default_storage_engine
MyISAM
SET @@session.default_storage_engine = INNODB;
SET @@session.default_storage_engine = DEFAULT;
SELECT @@session.default_storage_engine;
@@session.default_storage_engine
MyISAM
'#--------------------FN_DYNVARS_005_02-------------------------#'
SET @@global.default_storage_engine = MYISAM;
SELECT @@global.default_storage_engine;
@@global.default_storage_engine
MyISAM
'#--------------------FN_DYNVARS_005_03-------------------------#'
SET @@session.default_storage_engine = MYISAM;
SELECT @@session.default_storage_engine;
@@session.default_storage_engine
MyISAM
'#---------------------FN_DYNVARS_001_8----------------------#'
SET @@default_storage_engine = MYISAM;
SELECT @@default_storage_engine = @@local.default_storage_engine;
@@default_storage_engine = @@local.default_storage_engine
1
SELECT @@local.default_storage_engine = @@session.default_storage_engine;
@@local.default_storage_engine = @@session.default_storage_engine
1
SET @@default_storage_engine = @start_global_value;
SET @@global.default_storage_engine = @start_global_value;
SELECT @@global.default_storage_engine;
@@global.default_storage_engine
MyISAM
SET @@session.default_storage_engine = @start_session_value;
SELECT @@session.default_storage_engine;
@@session.default_storage_engine
MyISAM
