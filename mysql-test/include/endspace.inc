select 'a' = 'a', 'a' = 'a ', 'a ' = 'a';
select 'a\0' = 'a', 'a\0' < 'a', 'a\0' > 'a';
select 'a' = 'a\0', 'a' < 'a\0', 'a' > 'a\0';
select 'a\0' = 'a ', 'a\0' < 'a ', 'a\0' > 'a ';
select 'a ' = 'a\0', 'a ' < 'a\0', 'a ' > 'a\0';
select 'a  a' > 'a', 'a  \0' < 'a';
select binary 'a  a' > 'a', binary 'a  \0' > 'a', binary 'a\0' > 'a';
