use test;
drop table if exists mpk1;
create table if not exists mpk1 (
  id int not null primary key,
  k1 int,
  k2 smallint, 
  k3 mediumint unsigned,
  txt varchar(20),
  index idx2(k1, k2),
  index idx3(k3, k2, k1)
);


INSERT INTO mpk1 
   VALUES (1,  NULL, 1,    1001, "One"),
          (2,  NULL, 2,    1002, "Two"),
          (3,  NULL, 3,    1003, "Three"),
          (4,  1,    NULL, 1004, "Four"),
          (5,  1,    1,    1005, "Five"),
          (6,  1,    2,    1006, "Six"),
          (7,  1,    3,    1007, "Seven"),
          (8,  2,    NULL, 1008, "Eight"),
          (9,  2,    1,    1009, "Nine"),
          (10, 2,    2,    1010, "Ten"),
          (11, 2,    3,    1011, "Eleven"),
          (12, 3,    NULL, 1012, "Twelve"),
          (13, 3,    1,    1013, "Thirteen"),
          (14, 3,    2,    1014, "Fourteen"),
          (15, 3,    3,    1015, "Fifteen"),
          (16, 4,    NULL, NULL, "Sixteen"),
          (17, 4,    1,    NULL, "Seventeen"),
          (18, 4,    2,    NULL, "Eighteen"),
          (19, 4,    3,    NULL, "Nineteen"),
          (20, 5,    NULL, NULL, "Twenty"),
          (21, NULL, NULL, NULL, "Twenty One");

          