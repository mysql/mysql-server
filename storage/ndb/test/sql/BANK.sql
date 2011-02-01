-- Copyright (C) 2005 MySQL AB
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
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

CREATE DATABASE IF NOT EXISTS BANK default charset=latin1 default collate=latin1_bin;
USE BANK;
DROP TABLE IF EXISTS GL;
CREATE TABLE GL ( TIME BIGINT UNSIGNED NOT NULL,
                  ACCOUNT_TYPE INT UNSIGNED NOT NULL,
                  BALANCE INT UNSIGNED NOT NULL,
                  DEPOSIT_COUNT INT UNSIGNED NOT NULL,
                  DEPOSIT_SUM INT UNSIGNED NOT NULL,
                  WITHDRAWAL_COUNT INT UNSIGNED NOT NULL,
                  WITHDRAWAL_SUM INT UNSIGNED NOT NULL,
                  PURGED INT UNSIGNED NOT NULL,
                  PRIMARY KEY USING HASH (TIME,ACCOUNT_TYPE))
   ENGINE = NDB;

DROP TABLE IF EXISTS ACCOUNT;
CREATE TABLE ACCOUNT ( ACCOUNT_ID INT UNSIGNED NOT NULL,
                       OWNER INT UNSIGNED NOT NULL,
                       BALANCE INT UNSIGNED NOT NULL,
                       ACCOUNT_TYPE INT UNSIGNED NOT NULL,
                       PRIMARY KEY USING HASH (ACCOUNT_ID))
   ENGINE = NDB;

DROP TABLE IF EXISTS TRANSACTION;
CREATE TABLE TRANSACTION ( TRANSACTION_ID BIGINT UNSIGNED NOT NULL,
                           ACCOUNT INT UNSIGNED NOT NULL,
                           ACCOUNT_TYPE INT UNSIGNED NOT NULL,
                           OTHER_ACCOUNT INT UNSIGNED NOT NULL,
                           TRANSACTION_TYPE INT UNSIGNED NOT NULL,
                           TIME BIGINT UNSIGNED NOT NULL,
                           AMOUNT INT UNSIGNED NOT NULL,
                           PRIMARY KEY USING HASH (TRANSACTION_ID,ACCOUNT))
   ENGINE = NDB;

DROP TABLE IF EXISTS SYSTEM_VALUES;
CREATE TABLE SYSTEM_VALUES ( SYSTEM_VALUES_ID INT UNSIGNED NOT NULL,
                             VALUE BIGINT UNSIGNED NOT NULL,
                             PRIMARY KEY USING HASH (SYSTEM_VALUES_ID))
   ENGINE = NDB;

DROP TABLE IF EXISTS ACCOUNT_TYPE;
CREATE TABLE ACCOUNT_TYPE ( ACCOUNT_TYPE_ID INT UNSIGNED NOT NULL,
                            DESCRIPTION CHAR(64) NOT NULL,
                            PRIMARY KEY USING HASH (ACCOUNT_TYPE_ID))
   ENGINE = NDB;
