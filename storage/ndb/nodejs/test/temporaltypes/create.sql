- Copyright (c) 2013, 2020, Oracle and/or its affiliates.
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; version 2 of the License.
--
-- This program is also distributed with certain software (including
-- but not limited to OpenSSL) that is licensed under separate terms,
-- as designated in a particular file or component or in included license
-- documentation.  The authors of MySQL hereby grant you an additional
-- permission to link the program and your derivative works with the
-- separately licensed software that they have included with MySQL.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

use test;
drop table if exists temporaltypes;
create table if not exists temporaltypes (
  id int not null PRIMARY KEY,
  cYear year,
  index (cYear),
  cTimestamp timestamp NOT NULL,
  index (cTimestamp),
  cNullableTimestamp timestamp NULL,
  cTime time,
  index (cTime),
  cDate date,
  index (cDate),
  cDatetime datetime,
  index (cDatetime),
  cDatetimeDefault datetime NOT NULL DEFAULT "1989-11-09 17:00:00"
);
-- id year  cTimestamp             cNullableTimestamp     cTime       dDate         cDatetime              cDatetimeDefault
insert into temporaltypes values
  (1, 2001, '2001-01-01 01:01:01', '2001-01-01 01:01:01', '01:01:01', '2001-01-01', '2001-01-01 01:01:01', '2001-01-01 01:01:01');
insert into temporaltypes values
  (2, 2002, '2002-02-02 02:02:02', '2002-02-02 02:02:02', '02:02:02', '2002-02-02', '2002-02-02 02:02:02', '2002-02-02 02:02:02');
insert into temporaltypes values
  (3, 2003, '2003-03-03 03:03:03', '2003-03-03 03:03:03', '03:03:03', '2003-03-03', '2003-03-03 03:03:03', '2003-03-03 03:03:03');
insert into temporaltypes values
  (4, 2004, '2004-04-04 04:04:04', '2004-04-04 04:04:04', '04:04:04', '2004-04-04', '2004-04-04 04:04:04', '2004-04-04 04:04:04');
insert into temporaltypes values
  (5, 2005, '2005-05-05 05:05:05', '2005-05-05 05:05:05', '05:05:05', '2005-05-05', '2005-05-05 05:05:05', '2005-05-05 05:05:05');
insert into temporaltypes values
  (6, 2006, '2006-06-06 06:06:06', '2006-06-06 06:06:06', '06:06:06', '2006-06-06', '2006-06-06 06:06:06', '2006-06-06 06:06:06');
insert into temporaltypes values
  (7, 2007, '2007-07-07 07:07:07', '2007-07-07 07:07:07', '07:07:07', '2007-07-07', '2007-07-07 07:07:07', '2007-07-07 07:07:07');
insert into temporaltypes values
  (8, 2008, '2008-08-08 08:08:08', '2008-08-08 08:08:08', '08:08:08', '2008-08-08', '2008-08-08 08:08:08', '2008-08-08 08:08:08');
insert into temporaltypes values
  (9, 2009, '2009-09-09 09:09:09', '2009-09-09 09:09:09', '09:09:09', '2009-09-09', '2009-09-09 09:09:09', '2009-09-09 09:09:09');
insert into temporaltypes values
  (10, 2010, '2010-10-10 10:10:10', '2010-10-10 10:10:10', '10:10:10', '2010-10-10', '2010-10-10 10:10:10', '2010-10-10 10:10:10');
