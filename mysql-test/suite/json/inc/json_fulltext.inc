--echo #
--echo # Full Text Search on columns generated from JSON
--echo #

set names utf8;

create table messages
(
 id int,
 raw_message json,
 sender varchar(50) generated always as (json_unquote( json_extract( raw_message, '$.sender' ) )) stored,
 receiver varchar(50) generated always as (json_unquote( json_extract( raw_message, '$.receiver' ) )) stored,
 subject text generated always as (json_extract( raw_message, '$.subject' )) stored,
 received datetime generated always as (json_unquote( json_extract( raw_message, '$.received' ) )) stored,
 body text generated always as (json_extract( raw_message, '$.body' )) stored,
 fulltext key( subject ),
 fulltext key( body ),
 primary key( id ),
 key( received, sender, receiver ),
 key( sender, receiver, received )
);
show create table messages;

insert into messages(id, raw_message) values
(
  1,
  '{
     "sender": "fred",
     "receiver": "alice",
     "subject": "lunch today?",
     "received": "2015-05-11 09:30:05",
     "body": "How about lunch around 11:30? Thai food sounds good to me."
  }'
),
(
  2,
  '{
     "sender": "alice",
     "receiver": "fred",
     "subject": "re: lunch today?",
     "received": "2015-05-11 09:45:05",
     "body": "Great! No big deadline today."
  }'
),
(
  3,
  '{
     "sender": "fred",
     "receiver": "alice",
     "subject": "tea at 4:00?",
     "received": "2015-05-11 14:30:05",
     "body": "I have some yummy scones."
  }'
),
(
  4,
  '{
     "sender": "alice",
     "receiver": "fred",
     "subject": "re: tea at 4:00?",
     "received": "2015-05-11 14:45:05",
     "body": "Yes indeed."
  }'
),
(
  5,
  '{
     "sender": "fred",
     "receiver": "alice",
     "subject": "lunch today?",
     "received": "2015-05-18 09:30:05",
     "body": "Chinese for lunch?"
  }'
),
(
  6,
  '{
     "sender": "alice",
     "receiver": "fred",
     "subject": "re: lunch today?",
     "received": "2015-05-18 09:45:05",
     "body": "Sorry. I have a tight deadline I have to meet."
  }'
)
;

analyze table messages;

select * from messages order by id;

# FULLTEXT INDEX can't be created on JSON column type.
--error ER_BAD_FT_COLUMN
alter table messages add fulltext index (raw_message) ;

# Search the message subject and body
select id from messages
  where match( body ) against ( 'thai' )
  and match( subject ) against( 'lunch' )
  order by id;
select id from messages
  where match( body ) against ( 'deadline' )
  and received > timestamp '2015-05-13 09:45:05'
  order by id;

# Search query with condition involving FTS clause and JSON function
select id from messages
  where match( body ) against ( 'thai' )
  and match( subject ) against( 'lunch' )
  and JSON_VALID(raw_message)
  and ( JSON_TYPE(raw_message) = "OBJECT")
  order by id;

select id from messages
  where match( body ) against ( 'thai' )
  and match( subject ) against( 'lunch' )
  and ( JSON_TYPE(JSON_KEYS(raw_message)) != "ARRAY"  or JSON_CONTAINS (raw_message,CAST('{"received": "2015-05-11 09:30:05"}' AS JSON) ) )
  order by id;

select id from messages
  where match( subject ) against( '+tea' in boolean mode)
  and LENGTH(JSON_UNQUOTE(JSON_EXTRACT( raw_message, '$.sender' ))) = 5
  order by id;

select id from messages
   where match( body ) against( 'lunch' with query expansion)
   and JSON_DEPTH(raw_message) != 0;

# update the message body
update messages
  set raw_message = json_replace(raw_message, '$.body', 'On second thought, I want Chinese.')
where id=1;
select * from messages order by id;
select id from messages
  where match( body ) against ( 'thai' )
  and match( subject ) against( 'lunch' )
  order by id;

drop table messages;

