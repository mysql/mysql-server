CREATE TABLE bug32529561_v (
`longstr` blob NOT NULL,
`gen_1` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)),
`gen_2` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)),
`gen_3` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)),
`gen_4` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)),
`gen_5` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)),
KEY `key_5` (`gen_5`)
);
insert into bug32529561_v (longstr) values ('123');
select * from bug32529561_v;
longstr	gen_1	gen_2	gen_3	gen_4	gen_5
123	123	123	123	123	123
begin;
update bug32529561_v set longstr = concat('456', repeat('a', 9000));
rollback;
select * from bug32529561_v;
longstr	gen_1	gen_2	gen_3	gen_4	gen_5
123	123	123	123	123	123
CREATE TABLE bug32529561_s (
`longstr` blob NOT NULL,
`gen_1` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
`gen_2` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
`gen_3` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
`gen_4` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
`gen_5` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
KEY `key_5` (`gen_5`)
);
insert into bug32529561_s (longstr) values ('123');
select * from bug32529561_s;
longstr	gen_1	gen_2	gen_3	gen_4	gen_5
123	123	123	123	123	123
begin;
update bug32529561_s set longstr = concat('456', repeat('a', 9000));
rollback;
select * from bug32529561_s;
longstr	gen_1	gen_2	gen_3	gen_4	gen_5
123	123	123	123	123	123
CREATE TABLE bug32529561_m (
`longstr` blob NOT NULL,
`gen_1` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
`gen_2` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)),
`gen_3` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
`gen_4` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)),
`gen_5` varchar(3) GENERATED ALWAYS AS (left(`longstr`, 3)) stored,
KEY `key_5` (`gen_5`)
);
insert into bug32529561_m (longstr) values ('123');
select * from bug32529561_m;
longstr	gen_1	gen_2	gen_3	gen_4	gen_5
123	123	123	123	123	123
begin;
update bug32529561_m set longstr = concat('456', repeat('a', 9000));
rollback;
select * from bug32529561_m;
longstr	gen_1	gen_2	gen_3	gen_4	gen_5
123	123	123	123	123	123
DROP TABLE bug32529561_v;
DROP TABLE bug32529561_s;
DROP TABLE bug32529561_m;
