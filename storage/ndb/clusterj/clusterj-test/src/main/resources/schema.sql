# the first statement is a drop table for the test table
drop table if exists t_basic;
# the second statement is a test; if it succeeds, skip the rest of the file.
select id from t_basic where id = 9999;
# the following statements are delimited by semicolon

DROP TABLE IF EXISTS twopk;
CREATE TABLE IF NOT EXISTS twopk (
  id int not null,
  name varchar(30)
) ENGINE = ndbcluster;

DROP TABLE IF EXISTS subscriber ;

CREATE TABLE IF NOT EXISTS subscriber (
  imsi   VARCHAR(9) NOT NULL ,
  guti   VARCHAR(10) NOT NULL ,
  mme_s1ap_id   INT NOT NULL ,
  enb_s1ap_id   INT NOT NULL ,
  mme_teid   INT NULL ,
  sgw_teid   VARCHAR(39) NULL ,
  pgw_teid   VARCHAR(39) NULL ,
  imei   VARCHAR(9) NOT NULL ,
  msisdn   VARCHAR(11) NOT NULL ,
  ecm_state   CHAR NULL ,
  emm_state   CHAR NULL ,
  eps_cgi   VARCHAR(7) NULL ,
  global_enb_id   VARCHAR(7) NULL ,
  bearer_id   CHAR NULL ,
  sgw_ip_addr   VARCHAR(34) NULL ,
PRIMARY KEY (  imsi  ) ,
UNIQUE INDEX   imsi_UNIQUE   USING BTREE (  imsi   ASC) ,
UNIQUE INDEX   guti_UNIQUE   USING BTREE (  guti   ASC) ,
UNIQUE INDEX   mme_s1ap_id_UNIQUE   (  mme_s1ap_id   ASC) ,
UNIQUE INDEX   imei_UNIQUE   (  imei   ASC) ,
UNIQUE INDEX   msisdn_UNIQUE   (  msisdn   ASC) )
ENGINE = ndbcluster;

drop table if exists longlongstringpk;
create table longlongstringpk (
 longpk1 bigint not null,
 longpk2 bigint not null,
 stringpk varchar(10) not null,
 stringvalue varchar(10),
        CONSTRAINT PK_longlongstringpk PRIMARY KEY (longpk1, longpk2, stringpk)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists bittypes;
create table bittypes (
 id int not null primary key,

 bit1 bit(1),
 bit2 bit(2),
 bit4 bit(4),
 bit8 bit(8),
 bit16 bit(16),
 bit32 bit(32),
 bit64 bit(64)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists binarytypes;
create table binarytypes (
 id int not null primary key,

 binary1 binary(1),
 binary2 binary(2),
 binary4 binary(4),
 binary8 binary(8),
 binary16 binary(16),
 binary32 binary(32),
 binary64 binary(64),
 binary128 binary(128)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists varbinarytypes;
create table varbinarytypes (
 id int not null primary key,

 binary1 varbinary(1),
 binary2 varbinary(2),
 binary4 varbinary(4),
 binary8 varbinary(8),
 binary16 varbinary(16),
 binary32 varbinary(32),
 binary64 varbinary(64),
 binary128 varbinary(128),
 binary256 varbinary(256),
 binary512 varbinary(512),
 binary1024 varbinary(1024),
 binary2048 varbinary(2048)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists binarypk;
create table binarypk (
 id binary(255) primary key not null,
 number int not null,
 name varchar(10) not null
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists varbinarypk;
create table varbinarypk (
 id varbinary(255) primary key not null,
 number int not null,
 name varchar(10) not null
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists longvarbinarypk;
create table longvarbinarypk (
 id varbinary(256) primary key not null,
 number int not null,
 name varchar(10) not null
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists charsetlatin1;
create table charsetlatin1 (
 id int not null primary key,
 smallcolumn varchar(200),
 mediumcolumn varchar(500),
 largecolumn text(10000)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;
 
drop table if exists charsetbig5;
create table charsetbig5 (
 id int not null primary key,
 smallcolumn varchar(200),
 mediumcolumn varchar(500),
 largecolumn text(10000)

) ENGINE=ndbcluster DEFAULT CHARSET=big5;
 
drop table if exists charsetutf8;
create table charsetutf8 (
 id int not null primary key,
 smallcolumn varchar(200),
 mediumcolumn varchar(500),
 largecolumn text(10000)

) ENGINE=ndbcluster DEFAULT CHARSET=utf8;
 
drop table if exists charsetsjis;
create table charsetsjis (
 id int not null primary key,
 smallcolumn varchar(200),
 mediumcolumn varchar(500),
 largecolumn text(10000)

) ENGINE=ndbcluster DEFAULT CHARSET=sjis;
 
drop table if exists longintstringpk;
create table longintstringpk (
 longpk bigint not null,
 intpk int not null,
 stringpk varchar(10) not null,
 stringvalue varchar(10),
        CONSTRAINT PK_longlongstringpk PRIMARY KEY (longpk, intpk, stringpk)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1 PARTITION BY KEY(intpk, longpk);

drop table if exists hashonlylongintstringpk;
create table hashonlylongintstringpk (
 longpk bigint not null,
 intpk int not null,
 stringpk varchar(10) not null,
 stringvalue varchar(10),
        CONSTRAINT PK_longlongstringpk PRIMARY KEY (longpk, intpk, stringpk) USING HASH

) ENGINE=ndbcluster DEFAULT CHARSET=latin1 PARTITION BY KEY(intpk, longpk);

drop table if exists longlongstringfk;
create table longlongstringfk (
 longpk1 bigint not null,
 longpk2 bigint not null,
 stringpk varchar(10) not null,
 longfk1 bigint,
 longfk2 bigint,
 stringfk varchar(10),
 stringvalue varchar(10),
        KEY FK_longfk1longfk2stringfk (longfk1, longfk2, stringfk),
        CONSTRAINT PK_longlongstringfk PRIMARY KEY (longpk1, longpk2, stringpk)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists longintstringfk;
create table longintstringfk (
 longpk bigint not null,
 intpk int not null,
 stringpk varchar(10) not null,
 longfk bigint,
 intfk int,
 stringfk varchar(10),
 stringvalue varchar(10),
        KEY FK_longfkintfkstringfk (longfk, intfk, stringfk),
        CONSTRAINT PK_longintstringfk PRIMARY KEY (longpk, intpk, stringpk)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists a;
CREATE TABLE a (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        cstring         VARCHAR(100),
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists b0;
CREATE TABLE b0 (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        a_id            INT,
        cstring         VARCHAR(100),
        cvarchar_ascii  VARCHAR(100),
        ctext_ascii     TEXT(100),
        cvarchar_ucs2   VARCHAR(100),
        ctext_ucs2      TEXT(100),
        bytes           VARBINARY(202),
        KEY FK_a_id (a_id),
        CONSTRAINT PK_B0_0 PRIMARY KEY (id)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists blobtypes;
create table blobtypes (
 id int not null primary key,

 blobbytes blob,
 blobstream blob

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists bytestype;
create table bytestype (
 id int not null primary key,

 bytes_null_hash varbinary(8),
 bytes_null_btree varbinary(8),
 bytes_null_both varbinary(8),
 bytes_null_none varbinary(8),
key idx_bytes_null_btree (bytes_null_btree),
unique key idx_bytes_null_both (bytes_null_both),
unique key idx_bytes_null_hash (bytes_null_hash) using hash

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists doubletypes;
create table doubletypes (
 id int not null primary key,

 double_null_hash double,
 double_null_btree double,
 double_null_both double,
 double_null_none double,

 double_not_null_hash double,
 double_not_null_btree double,
 double_not_null_both double,
 double_not_null_none double,
 unique key idx_double_null_hash (double_null_hash) using hash,
 key idx_double_null_btree (double_null_btree),
 unique key idx_double_null_both (double_null_both),

 unique key idx_double_not_null_hash (double_not_null_hash) using hash,
 key idx_double_not_null_btree (double_not_null_btree),
 unique key idx_double_not_null_both (double_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists localetypes;
create table localetypes (
 id int not null primary key,

 locale_null_hash varchar(20),
 locale_null_btree varchar(300),
 locale_null_both varchar(20),
 locale_null_none varchar(300),

 locale_not_null_hash varchar(300),
 locale_not_null_btree varchar(20),
 locale_not_null_both varchar(300),
 locale_not_null_none varchar(20),
 unique key idx_locale_null_hash (locale_null_hash) using hash,
 key idx_locale_null_btree (locale_null_btree),
 unique key idx_locale_null_both (locale_null_both),

 unique key idx_locale_not_null_hash (locale_not_null_hash) using hash,
 key idx_locale_not_null_btree (locale_not_null_btree),
 unique key idx_locale_not_null_both (locale_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists stringtypes;
create table stringtypes (
 id int not null primary key,

 string_null_hash varchar(20),
 string_null_btree varchar(300),
 string_null_both varchar(20),
 string_null_none varchar(300),

 string_not_null_hash varchar(300),
 string_not_null_btree varchar(20),
 string_not_null_both varchar(300),
 string_not_null_none varchar(20),
 unique key idx_string_null_hash (string_null_hash) using hash,
 key idx_string_null_btree (string_null_btree),
 unique key idx_string_null_both (string_null_both),

 unique key idx_string_not_null_hash (string_not_null_hash) using hash,
 key idx_string_not_null_btree (string_not_null_btree),
 unique key idx_string_not_null_both (string_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists floattypes;
create table floattypes (
 id int not null primary key,

 float_null_hash float,
 float_null_btree float,
 float_null_both float,
 float_null_none float,

 float_not_null_hash float,
 float_not_null_btree float,
 float_not_null_both float,
 float_not_null_none float,
 unique key idx_float_null_hash (float_null_hash) using hash,
 key idx_float_null_btree (float_null_btree),
 unique key idx_float_null_both (float_null_both),

 unique key idx_float_not_null_hash (float_not_null_hash) using hash,
 key idx_float_not_null_btree (float_not_null_btree),
 unique key idx_float_not_null_both (float_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists t_basic;
create table t_basic (
  id int not null,
  name varchar(32),
  age int,
  magic int not null,
  primary key(id),

  unique key idx_unique_hash_magic (magic) using hash,
  key idx_btree_age (age)
) ENGINE=ndbcluster;

drop table if exists dn2id;
create table dn2id (
 eid bigint(20) unsigned NOT NULL,
 object_classes varchar(100) NOT NULL,
 x_object_classes varchar(100) NOT NULL DEFAULT '',
 a0 varchar(128) NOT NULL DEFAULT '',
 a1 varchar(128) NOT NULL DEFAULT '',
 a2 varchar(128) NOT NULL DEFAULT '',
 a3 varchar(128) NOT NULL DEFAULT '',
 a4 varchar(128) NOT NULL DEFAULT '',
 a5 varchar(128) NOT NULL DEFAULT '',
 a6 varchar(128) NOT NULL DEFAULT '',
 a7 varchar(128) NOT NULL DEFAULT '',
 a8 varchar(128) NOT NULL DEFAULT '',
 a9 varchar(128) NOT NULL DEFAULT '',
 a10 varchar(128) NOT NULL DEFAULT '',
 a11 varchar(128) NOT NULL DEFAULT '',
 a12 varchar(128) NOT NULL DEFAULT '',
 a13 varchar(128) NOT NULL DEFAULT '',
 a14 varchar(128) NOT NULL DEFAULT '',
 a15 varchar(128) NOT NULL DEFAULT '',
 PRIMARY KEY (a0,a1,a2,a3,a4,a5,a6,a7,a8,a9,a10,a11,a12,a13,a14,a15),
 unique key idx_unique_hash_eid (eid) using hash
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists nullvalues;
create table nullvalues (
 id int not null primary key,
 int_not_null_default_null_value_default int not null default '5',
 int_not_null_default_null_value_exception int not null default '5',
 int_not_null_default_null_value_none int not null default '5',
 int_not_null_no_default_null_value_default int not null,
 int_not_null_no_default_null_value_exception int not null,
 int_not_null_no_default_null_value_none int not null,
 int_null_default_null_value_default int default '5',
 int_null_default_null_value_exception int default '5',
 int_null_default_null_value_none int default '5',
 int_null_no_default_null_value_default int,
 int_null_no_default_null_value_exception int,
 int_null_no_default_null_value_none int,

 long_not_null_default_null_value_default bigint not null default '5',
 long_not_null_default_null_value_exception bigint not null default '5',
 long_not_null_default_null_value_none bigint not null default '5',
 long_not_null_no_default_null_value_default bigint not null,
 long_not_null_no_default_null_value_exception bigint not null,
 long_not_null_no_default_null_value_none bigint not null,
 long_null_default_null_value_default bigint default '5',
 long_null_default_null_value_exception bigint default '5',
 long_null_default_null_value_none bigint default '5',
 long_null_no_default_null_value_default bigint,
 long_null_no_default_null_value_exception bigint,
 long_null_no_default_null_value_none bigint,

 short_not_null_default_null_value_default smallint not null default '5',
 short_not_null_default_null_value_exception smallint not null default '5',
 short_not_null_default_null_value_none smallint not null default '5',
 short_not_null_no_default_null_value_default smallint not null,
 short_not_null_no_default_null_value_exception smallint not null,
 short_not_null_no_default_null_value_none smallint not null,
 short_null_default_null_value_default smallint default '5',
 short_null_default_null_value_exception smallint default '5',
 short_null_default_null_value_none smallint default '5',
 short_null_no_default_null_value_default smallint,
 short_null_no_default_null_value_exception smallint,
 short_null_no_default_null_value_none smallint,

 byte_not_null_default_null_value_default tinyint not null default '5',
 byte_not_null_default_null_value_exception tinyint not null default '5',
 byte_not_null_default_null_value_none tinyint not null default '5',
 byte_not_null_no_default_null_value_default tinyint not null,
 byte_not_null_no_default_null_value_exception tinyint not null,
 byte_not_null_no_default_null_value_none tinyint not null,
 byte_null_default_null_value_default tinyint default '5',
 byte_null_default_null_value_exception tinyint default '5',
 byte_null_default_null_value_none tinyint default '5',
 byte_null_no_default_null_value_default tinyint,
 byte_null_no_default_null_value_exception tinyint,
 byte_null_no_default_null_value_none tinyint,

 string_not_null_default_null_value_default varchar(5) not null default '5',
 string_not_null_default_null_value_exception varchar(5) not null default '5',
 string_not_null_default_null_value_none varchar(5) not null default '5',
 string_not_null_no_default_null_value_default varchar(5) not null,
 string_not_null_no_default_null_value_exception varchar(5) not null,
 string_not_null_no_default_null_value_none varchar(5) not null,
 string_null_default_null_value_default varchar(5) default '5',
 string_null_default_null_value_exception varchar(5) default '5',
 string_null_default_null_value_none varchar(5) default '5',
 string_null_no_default_null_value_default varchar(5),
 string_null_no_default_null_value_exception varchar(5),
 string_null_no_default_null_value_none varchar(5),

 float_not_null_default_null_value_default float not null default '5',
 float_not_null_default_null_value_exception float not null default '5',
 float_not_null_default_null_value_none float not null default '5',
 float_not_null_no_default_null_value_default float not null,
 float_not_null_no_default_null_value_exception float not null,
 float_not_null_no_default_null_value_none float not null,
 float_null_default_null_value_default float default '5',
 float_null_default_null_value_exception float default '5',
 float_null_default_null_value_none float default '5',
 float_null_no_default_null_value_default float,
 float_null_no_default_null_value_exception float,
 float_null_no_default_null_value_none float,

 double_not_null_default_null_value_default double not null default '5',
 double_not_null_default_null_value_exception double not null default '5',
 double_not_null_default_null_value_none double not null default '5',
 double_not_null_no_default_null_value_default double not null,
 double_not_null_no_default_null_value_exception double not null,
 double_not_null_no_default_null_value_none double not null,
 double_null_default_null_value_default double default '5',
 double_null_default_null_value_exception double default '5',
 double_null_default_null_value_none double default '5',
 double_null_no_default_null_value_default double,
 double_null_no_default_null_value_exception double,
 double_null_no_default_null_value_none double

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists allprimitives;
create table allprimitives (
 id int not null primary key,

 int_not_null_hash int not null,
 int_not_null_btree int not null,
 int_not_null_both int not null,
 int_not_null_none int not null,
 int_null_hash int,
 int_null_btree int,
 int_null_both int,
 int_null_none int,

 byte_not_null_hash tinyint not null,
 byte_not_null_btree tinyint not null,
 byte_not_null_both tinyint not null,
 byte_not_null_none tinyint not null,
 byte_null_hash tinyint,
 byte_null_btree tinyint,
 byte_null_both tinyint,
 byte_null_none tinyint,

 short_not_null_hash smallint not null,
 short_not_null_btree smallint not null,
 short_not_null_both smallint not null,
 short_not_null_none smallint not null,
 short_null_hash smallint,
 short_null_btree smallint,
 short_null_both smallint,
 short_null_none smallint,

 long_not_null_hash bigint not null,
 long_not_null_btree bigint not null,
 long_not_null_both bigint not null,
 long_not_null_none bigint not null,
 long_null_hash bigint,
 long_null_btree bigint,
 long_null_both bigint,
 long_null_none bigint,

 unique key idx_int_not_null_hash (int_not_null_hash) using hash,
 key idx_int_not_null_btree (int_not_null_btree),
 unique key idx_int_not_null_both (int_not_null_both),
 unique key idx_int_null_hash (int_null_hash) using hash,
 key idx_int_null_btree (int_null_btree),
 unique key idx_int_null_both (int_null_both),

 unique key idx_byte_not_null_hash (byte_not_null_hash) using hash,
 key idx_byte_not_null_btree (byte_not_null_btree),
 unique key idx_byte_not_null_both (byte_not_null_both),
 unique key idx_byte_null_hash (byte_null_hash) using hash,
 key idx_byte_null_btree (byte_null_btree),
 unique key idx_byte_null_both (byte_null_both),

 unique key idx_short_not_null_hash (short_not_null_hash) using hash,
 key idx_short_not_null_btree (short_not_null_btree),
 unique key idx_short_not_null_both (short_not_null_both),
 unique key idx_short_null_hash (short_null_hash) using hash,
 key idx_short_null_btree (short_null_btree),
 unique key idx_short_null_both (short_null_both),

 unique key idx_long_not_null_hash (long_not_null_hash) using hash,
 key idx_long_not_null_btree (long_not_null_btree),
 unique key idx_long_not_null_both (long_not_null_both),
 unique key idx_long_null_hash (long_null_hash) using hash,
 key idx_long_null_btree (long_null_btree),
 unique key idx_long_null_both (long_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists decimaltypes;
create table decimaltypes (
 id int not null primary key,

 decimal_null_hash decimal(10,5),
 decimal_null_btree decimal(10,5),
 decimal_null_both decimal(10,5),
 decimal_null_none decimal(10,5),

 unique key idx_decimal_null_hash (decimal_null_hash) using hash,
 key idx_decimal_null_btree (decimal_null_btree),
 unique key idx_decimal_null_both (decimal_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists bigintegertypes;
create table bigintegertypes (
 id int not null primary key,

 decimal_null_hash decimal(10),
 decimal_null_btree decimal(10),
 decimal_null_both decimal(10),
 decimal_null_none decimal(10),

 unique key idx_decimal_null_hash (decimal_null_hash) using hash,
 key idx_decimal_null_btree (decimal_null_btree),
 unique key idx_decimal_null_both (decimal_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists timestamptypes;
create table timestamptypes (
 id int not null primary key,

 timestamp_not_null_hash timestamp not null,
 timestamp_not_null_btree timestamp not null,
 timestamp_not_null_both timestamp not null,
 timestamp_not_null_none timestamp not null,

 unique key idx_timestamp_not_null_hash (timestamp_not_null_hash) using hash,
 key idx_timestamp_not_null_btree (timestamp_not_null_btree),
 unique key idx_timestamp_not_null_both (timestamp_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists stringtype;
create table stringtype (
 id int not null primary key,

 string_null_hash varchar(10),
 string_null_btree varchar(10),
 string_null_both varchar(10),
 string_null_none varchar(10),

 unique key idx_string_null_hash (string_null_hash) using hash,
 key idx_string_null_btree (string_null_btree),
 unique key idx_string_null_both (string_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists yeartypes;
create table yeartypes (
 id int not null primary key,

 year_null_hash year,
 year_null_btree year,
 year_null_both year,
 year_null_none year,

 year_not_null_hash year,
 year_not_null_btree year,
 year_not_null_both year,
 year_not_null_none year,

 unique key idx_year_null_hash (year_null_hash) using hash,
 key idx_year_null_btree (year_null_btree),
 unique key idx_year_null_both (year_null_both),

 unique key idx_year_not_null_hash (year_not_null_hash) using hash,
 key idx_year_not_null_btree (year_not_null_btree),
 unique key idx_year_not_null_both (year_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists timetypes;
create table timetypes (
 id int not null primary key,

 time_null_hash time,
 time_null_btree time,
 time_null_both time,
 time_null_none time,

 time_not_null_hash time,
 time_not_null_btree time,
 time_not_null_both time,
 time_not_null_none time,

 unique key idx_time_null_hash (time_null_hash) using hash,
 key idx_time_null_btree (time_null_btree),
 unique key idx_time_null_both (time_null_both),

 unique key idx_time_not_null_hash (time_not_null_hash) using hash,
 key idx_time_not_null_btree (time_not_null_btree),
 unique key idx_time_not_null_both (time_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists datetypes;
create table datetypes (
 id int not null primary key,

 date_null_hash date,
 date_null_btree date,
 date_null_both date,
 date_null_none date,

 date_not_null_hash date,
 date_not_null_btree date,
 date_not_null_both date,
 date_not_null_none date,

 unique key idx_date_null_hash (date_null_hash) using hash,
 key idx_date_null_btree (date_null_btree),
 unique key idx_date_null_both (date_null_both),

 unique key idx_date_not_null_hash (date_not_null_hash) using hash,
 key idx_date_not_null_btree (date_not_null_btree),
 unique key idx_date_not_null_both (date_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists datetimetypes;
create table datetimetypes (
 id int not null primary key,

 datetime_null_hash datetime,
 datetime_null_btree datetime,
 datetime_null_both datetime,
 datetime_null_none datetime,

 datetime_not_null_hash datetime,
 datetime_not_null_btree datetime,
 datetime_not_null_both datetime,
 datetime_not_null_none datetime,

 unique key idx_datetime_null_hash (datetime_null_hash) using hash,
 key idx_datetime_null_btree (datetime_null_btree),
 unique key idx_datetime_null_both (datetime_null_both),

 unique key idx_datetime_not_null_hash (datetime_not_null_hash) using hash,
 key idx_datetime_not_null_btree (datetime_not_null_btree),
 unique key idx_datetime_not_null_both (datetime_not_null_both)

) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists longintstringix;
create table longintstringix (
 id int(11) not null,
 longix bigint(20) not null,
 stringix varchar(10) not null,
 intix int(11) not null,
 stringvalue varchar(10) default null,
 PRIMARY KEY (id),
 KEY idx_long_int_string (longix, intix, stringix)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists cassandra_string;
create table cassandra_string (
  id varchar(10),
  c1 varchar(34),
  c2 varchar(34),
  c3 varchar(34),
  c4 varchar(34),
  c5 varchar(34)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists cassandra_byte_array;
create table cassandra_byte_array (
  id binary(10) primary key,
  c1 binary(34),
  c2 binary(34),
  c3 binary(34),
  c4 binary(34),
  c5 binary(34)
) ENGINE=ndbcluster DEFAULT CHARSET=latin1;

drop table if exists stress;
create table stress (
  id int not null primary key,
  i0 int not null,
  l0 bigint not null,
  f0 float not null,
  d0 double not null,
  i1 int not null,
  l1 bigint not null,
  f1 float not null,
  d1 double not null,
  i2 int not null,
  l2 bigint not null,
  f2 float not null,
  d2 double not null,
  i3 int not null,
  l3 bigint not null,
  f3 float not null,
  d3 double not null,
  i4 int not null,
  l4 bigint not null,
  f4 float not null,
  d4 double not null,
  i5 int not null,
  l5 bigint not null,
  f5 float not null,
  d5 double not null,
  i6 int not null,
  l6 bigint not null,
  f6 float not null,
  d6 double not null,
  i7 int not null,
  l7 bigint not null,
  f7 float not null,
  d7 double not null,
  i8 int not null,
  l8 bigint not null,
  f8 float not null,
  d8 double not null,
  i9 int not null,
  l9 bigint not null,
  f9 float not null,
  d9 double not null,
  i10 int not null,
  l10 bigint not null,
  f10 float not null,
  d10 double not null,
  i11 int not null,
  l11 bigint not null,
  f11 float not null,
  d11 double not null,
  i12 int not null,
  l12 bigint not null,
  f12 float not null,
  d12 double not null,
  i13 int not null,
  l13 bigint not null,
  f13 float not null,
  d13 double not null,
  i14 int not null,
  l14 bigint not null,
  f14 float not null,
  d14 double not null,
  i15 int not null,
  l15 bigint not null,
  f15 float not null,
  d15 double not null,
  i16 int not null,
  l16 bigint not null,
  f16 float not null,
  d16 double not null,
  i17 int not null,
  l17 bigint not null,
  f17 float not null,
  d17 double not null,
  i18 int not null,
  l18 bigint not null,
  f18 float not null,
  d18 double not null,
  i19 int not null,
  l19 bigint not null,
  f19 float not null,
  d19 double not null
  ) ENGINE=ndbcluster;

create database if not exists test2;
use test2;
drop table if exists t_basic2;
create table t_basic2 (
  id int not null,
  name varchar(32),
  age int,
  magic int not null,
  primary key(id),

  unique key idx_unique_hash_magic (magic) using hash,
  key idx_btree_age (age)
) ENGINE=ndbcluster;
use test;
