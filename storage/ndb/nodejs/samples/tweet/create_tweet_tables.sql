
--  Simple twitter-like appliction
--
--  Supports:
--     Users (author table)
--     Tweets (tweet table)
--     @user references (mention table)
--     Hashtags (hashtag table);
--     Followers (follow table).
--
--  Some notes about this schema:
--    Tweets have ascending auto-increment ids
--    timestamp(2) on tweet table is accurate to hundredths of a second
--    UTF16LE encoding of strings is also the native JavaScript encoding
--    Foreign key constraints maintain integrity of relationships,
--    and tables must be dropped in reverse order of dependencies

--  By default we create NDBCluster tables, so the demo can be used with
--  either the "ndb" or the "mysql" backend.  You can change this here to
--  use InnDB tables.

set default_storage_engine=ndbcluster;   # Use NDB
-- set default_storage_engine=innodb;    # Use InnoDB

use test;

DROP TABLE if exists follow;
DROP TABLE if exists hashtag;
DROP TABLE if exists mention;
DROP TABLE if exists tweet;
DROP TABLE if exists author;


CREATE TABLE author (
  user_name varchar(20) CHARACTER SET UTF16LE not null,
  full_name varchar(250),
  tweet_count int unsigned not null default 0,
  SPARSE_FIELDS varchar(4000) CHARACTER SET utf8,
  PRIMARY KEY(user_name)
) ;


CREATE TABLE tweet (
  id bigint unsigned auto_increment not null primary key,
  author_user_name varchar(20) CHARACTER SET UTF16LE,
  message varchar(140) CHARACTER SET UTF16LE,
  date_created timestamp(2), 
  KEY idx_btree_date(date_created),
  KEY idx_btree_author_date(author_user_name, date_created),
  CONSTRAINT author_fk FOREIGN KEY (author_user_name)
    REFERENCES author(user_name)
    ON DELETE CASCADE ON UPDATE RESTRICT
) ;


CREATE TABLE hashtag (
  hashtag varchar(20),
  tweet_id bigint unsigned,
  PRIMARY KEY(hashtag, tweet_id),
  CONSTRAINT tweet_fk FOREIGN KEY (tweet_id) REFERENCES tweet(id) 
    ON DELETE CASCADE ON UPDATE RESTRICT
) ;


CREATE TABLE mention (
  at_user varchar(20) CHARACTER SET UTF16LE,
  tweet_id bigint unsigned, 
  PRIMARY KEY (at_user, tweet_id),
  CONSTRAINT tweet_fk FOREIGN KEY (tweet_id) REFERENCES tweet(id) 
    ON DELETE CASCADE ON UPDATE RESTRICT  
) ;


CREATE TABLE follow (
  follower varchar(20) CHARACTER SET UTF16LE,
  followed varchar(20) CHARACTER SET UTF16LE,
  PRIMARY KEY (follower, followed),
  INDEX reverse_idx (followed, follower),
  CONSTRAINT follower_fk FOREIGN KEY (follower) REFERENCES author(user_name)
    ON DELETE CASCADE ON UPDATE RESTRICT,
  CONSTRAINT followed_fk FOREIGN KEY (followed) REFERENCES author(user_name) 
    ON DELETE CASCADE ON UPDATE RESTRICT  
) ;

