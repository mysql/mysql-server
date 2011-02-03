-- Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

-- SET storage_engine=MEMORY;
-- fails with
-- ERROR 1163 (42000) at line 24: The used table type doesn't support BLOB/TEXT columns

SET storage_engine=NDB;
-- SET storage_engine=INNODB;
-- SET storage_engine=MYISAM;

DROP DATABASE IF EXISTS crunddb;
CREATE DATABASE crunddb;
USE crunddb;

DROP TABLE IF EXISTS b1;
DROP TABLE IF EXISTS b0;
DROP TABLE IF EXISTS a;

CREATE TABLE a (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        CONSTRAINT PK_A_0 PRIMARY KEY (id)
);

CREATE TABLE b0 (
        id              INT             NOT NULL,
        cint            INT,
        clong           BIGINT,
        cfloat          FLOAT,
        cdouble         DOUBLE,
        a_id            INT,
 	-- crund code currently does not support VARBINARY/CHAR > 202
        cvarbinary_def  VARBINARY(202),
        cvarchar_def    VARCHAR(202),
        cblob_def       BLOB(1000004),
        ctext_def       TEXT(1000004),
        CONSTRAINT PK_B0_0 PRIMARY KEY (id),
        CONSTRAINT FK_B0_1 FOREIGN KEY (a_id) REFERENCES a (id)
);
--        cvarchar_ascii  VARCHAR(202) CHARACTER SET ASCII,
--        ctext_ascii     TEXT(202) CHARACTER SET ASCII,
--        cvarchar_ucs2   VARCHAR(202) CHARACTER SET UCS2,
--        ctext_ucs2      TEXT(202) CHARACTER SET UCS2,
--        cvarchar_utf8   VARCHAR(202) CHARACTER SET UTF8,
--        ctext_utf8      TEXT(202) CHARACTER SET UTF8,

CREATE TABLE b1 (
        id              INT NOT NULL,
        CONSTRAINT PK_B1_0 PRIMARY KEY (id)
);

CREATE INDEX I_B0_FK ON b0 (
        a_id
);


-- TEXT stores first 256 in primary table, rest in parts table at
-- different chunk sizes

-- VARBINARY/VARCHAR always store in primary tables, hence 8k limit == 8052B

-- 5.1 Reference Manual, Data Types (10)
-- Numeric Types:
-- * BIT[(M)] (1 <= M <= 64, default= 1, approximately (M+7)/8 bytes)
-- * BOOL, BOOLEAN (synonyms for TINYINT(1))
-- * INT[(M)], INTEGER[(M)] [UNSIGNED] (4 bytes)
--   TINYINT[(M)] (1 byte), SMALLINT[(M)] (2 bytes),
--   MEDIUMINT[(M)] (3 bytes), BIGINT[(M)] (8 bytes)
-- * FLOAT[(M,D)] [UNSIGNED] (4 bytes if 0 <= p <= 24, 8 bytes if 25 <= p <= 53)
-- * DOUBLE[(M,D)], DOUBLE[(M,D)], REAL[(M,D)] [UNSIGNED] (8 bytes)
-- * DECIMAL[(M[,D])],DEC[(M[,D])],NUMERIC[(M[,D])],FIXED[(M[,D])] [UNSIGNED]
--   (M*4/9 bytes plus extra)
-- Date Types:
-- * DATETIME (8 bytes)
-- * DATE (3 bytes, refman also says 4 bytes)
-- * TIME (3 bytes)
-- * TIMESTAMP (4 bytes)
-- * YEAR[(2|4)] (1 byte)
-- String Types:
-- * BINARY(M), [NATIONAL] CHAR[(M)] [CHARACTER SET name] [COLLATE name]
--   (fixed-length, 0 <= M <= 2^8-1, default=1, right-padded)
-- * VARBINARY(M), [NATIONAL] VARCHAR[(M)] [CHARACTER SET name] [COLLATE name]
--   (variable-length, 0 <= M <= 2^16-1, 1..2-byte length prefix)
-- * BLOB[(M)], TEXT[(M)] [CHARACTER SET name] [COLLATE name]
--   (max length = 2^16-1 bytes, 2-byte length prefix, no default values)
-- * TINYBLOB, TINYTEXT (1-byte length prefix),
-- * MEDIUMBLOB, MEDIUMTEXT (3-byte length prefix),
-- * LONGBLOB, LONGTEXT (4-byte length prefix)
-- * ENUM (max 2^16-1 elements, 1..2 bytes)
-- * SET (max 64 elements, 1,2,3,4, or 8 bytes)
-- varying size types subject to the maximum row size
-- character sizes subject to the character set (utf8 = 1..3 bytes/character)

-- The largest values of a BLOB or TEXT object that can be transmitted
-- between the client and server is determined by the amount of available
-- memory and the size of the communications buffers. You can change the
-- message buffer size by changing the value of the max_allowed_packet
-- variable, but you must do so for both the server and the client program.

-- Each BLOB or TEXT value is represented internally by a separately
-- allocated object. This is in contrast to all other data types, for which
-- storage is allocated once per column when the table is opened.

-- TEXT and BLOB columns are implemented differently in the NDB Cluster
-- storage engine, wherein each row in a TEXT column is made up of two
-- separate parts. One of these is of fixed size (256 bytes), and is
-- actually stored in the original table. The other consists of any data
-- in excess of 256 bytes, which is stored in a hidden table. The rows
-- in this second table are always 2,000 bytes long. This means that the
-- size of a TEXT column is 256 if size <= 256 (where size represents
-- the size of the row); otherwise, the size is 256 + size + (2000 –
-- (size – 256) % 2000).

-- For tables using the NDBCLUSTER storage engine, there is the factor of
-- 4-byte alignment to be taken into account when calculating storage
-- requirements. This means that all NDB data storage is done in multiples
-- of 4 bytes.  For example, in NDBCLUSTER tables, the TINYINT, SMALLINT,
-- MEDIUMINT, and INTEGER (INT) column types each require 4 bytes storage
-- per record due to the alignment factor.

-- When calculating storage requirements for MySQL Cluster tables, you must
-- also remember that every table using the NDBCLUSTER storage engine
-- requires a primary key; if no primary key is defined by the user, then a
-- “hidden” primary key will be created by NDB.  This hidden primary key
-- consumes 31-35 bytes per table record.

-- The NDBCLUSTER storage engine in MySQL 5.1 supports variable-width
-- columns. This means that a VARCHAR column in a MySQL Cluster table
-- requires the same amount of storage as it would using any other
-- storage engine, with the exception that such values are 4-byte
-- aligned. Thus, the string 'abcd' stored in a VARCHAR(50) column using
-- the latin1 character set requires 8 bytes (rather than 6 bytes for
-- the same column value in a MyISAM table). This represents a change in
-- behavior from earlier versions of NDB-CLUSTER, where a VARCHAR(50)
-- column would require 52 bytes storage per record regardless of the
-- length of the string being stored.

-- You may find the ndb_size.pl utility to be useful for estimating NDB
-- storage requirements. This Perl script connects to a current MySQL
-- (non-Cluster) database and creates a report on how much space that
-- database would require if it used the NDBCLUSTER storage engine. See
-- Section 16.10.15, "ndb_size.pl — NDBCLUSTER Size Requirement Estimator",
-- for more information.

-- 9.2. The Character Set Used for Data and Sorting
-- default: latin1 (cp1252 West European) character set and the
-- latin1_swedish_ci collation
-- CHARSET may be one of binary, armscii8, ascii, big5, cp1250, cp1251,
-- cp1256, cp1257, cp850, cp852, cp866, cp932, dec8, eucjpms, euckr,
-- gb2312, gbk, geostd8, greek, hebrew, hp8, keybcs2, koi8r, koi8u,
-- latin1, latin2, latin5, latin7, macce, macroman, sjis, swe7, tis620,
-- ucs2, ujis, utf8.

-- java.nio.charset.Charset:
-- The native character encoding of the Java programming language is UTF-16.
-- [not sure: 
--      out.println("default charset: " + java.nio.charset.Charset.defaultCharset().displayName());
--  shows: UTF-8
-- ]

-- http://unicode.org/faq/basic_q.html#14
-- Q: What is the difference between UCS-2 and UTF-16?
-- A: UCS-2 is what a Unicode implementation was up to Unicode 1.1,
-- before surrogate code points and UTF-16 were added as concepts to
-- Version 2.0 of the standard. This term should be now be avoided.
--
-- When interpreting what people have meant by "UCS-2" in past usage, it
-- is best thought of as not a data format, but as an indication that an
-- implementation does not interpret any supplementary characters. In
-- particular, for the purposes of data exchange, UCS-2 and UTF-16 are
-- identical formats. Both are 16-bit, and have exactly the same code
-- unit representation.
--
-- The effective difference between UCS-2 and UTF-16 lies at a different
-- level, when one is interpreting a sequence code units as code points
-- or as characters. In that case, a UCS-2 implementation would not
-- handle processing like character properties, codepoint boundaries,
-- collation, etc. for supplementary characters. [MD] & [KW]


-- 5.1 Reference Manual, NDB Limitations (16.14)
-- * no indexes on TEXT and BLOB colums; but VARCHAR allowed
-- * attribute names are truncated to 31 characters (errors if not unique)
-- * database names and table names can total a maximum of 122 characters
-- * past issues with table names having special characters
-- * the maximum number of NDB tables is limited to 20320
-- * the maximum number of attributes (columns and indexes) per table is
--   limited to 128
-- * the maximum number of attributes per key is 32
-- * the maximum permitted size of any one row is 8K; each BLOB or TEXT
--   column contributes 256 + 8 = 264 bytes towards this total
-- * the foreign key construct is ignored (just as it is in MyISAM tables)
--
-- * There are no durable commits on disk. Commits are replicated, but there
--   is no guarantee that logs are flushed to disk on commit.
-- * Range scans. There are query performance issues due to sequential access
--   to the NDB storage engine; it is also relatively more expensive to do
--   many range scans than it is with either MyISAM or InnoDB.
-- * Reliability of Records in range. The Records in range statistic is
--   available but is not completely tested or officially supported. This may
--   result in non-optimal query plans in some cases. If necessary, you can
--   employ USE INDEX or FORCE INDEX to alter the execution plan
-- * Unique hash indexes created with USING HASH cannot be used for accessing
--   a table if NULL is given as part of the key.
--
-- * No distributed table locks. A LOCK TABLES works only for the SQL node on
--   which the lock is issued; no other SQL node in the cluster “sees” this
--   lock.  For example, ALTER TABLE is not fully locking when running
--   multiple MySQL servers (SQL nodes).
--
-- * DDL operations. DDL operations (such as CREATE TABLE or ALTER TABLE) are
--   not safe from data node failures. If a data node fails while trying to
--   perform one of these, the data dictionary is locked and no further DDL
--   statements can be executed without restarting the cluster.
--
-- * The result of operations such as ALTER TABLE and CREATE INDEX performed
--   on one SQL node in the cluster are now visible to the cluster's other
--   SQL nodes without any additional action being taken.

-- quit;
