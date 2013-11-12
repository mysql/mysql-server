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
