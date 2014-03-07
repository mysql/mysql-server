use test;

--  Simple twitter-like appliction
--
--  Supports Users (author table); Tweets (tweet table); 
--  @user references (mention table); Hashtags (hashtag table); 
--  Followers (follow table).
--
--  Some notes about this schema:
--    Tweets have ascending auto-increment ids
--    timestamp(2) on tweet table is accurate to hundredths of a second
--    UTF16LE encoding of strings is also the native JavaScript encoding
--    Foreign key constraints maintain integrity of relationships


-- tables must be dropped in reverse order of dependencies

DROP TABLE if exists follow;
DROP TABLE if exists hashtag;
DROP TABLE if exists mention;
DROP TABLE if exists tweet;
DROP TABLE if exists author;


CREATE TABLE author (
  user_name varchar(20) CHARACTER SET UTF16LE not null,
  full_name varchar(250),
  tweets int unsigned not null default 0,
  PRIMARY KEY USING HASH(user_name)
) ENGINE=ndbcluster;


CREATE TABLE tweet (
  id bigint unsigned auto_increment not null primary key,
  author varchar(20) CHARACTER SET UTF16LE,
  message varchar(140) CHARACTER SET UTF16LE,
  date_created timestamp(2), 
  KEY idx_btree_date(date_created),
  KEY idx_btree_author_date(author, date_created),
  CONSTRAINT author_fk FOREIGN KEY (author) REFERENCES author(user_name) 
    ON DELETE CASCADE ON UPDATE RESTRICT
) ENGINE=ndbcluster;


CREATE TABLE hashtag (
  hashtag varchar(20),
  tweet_id bigint unsigned,
  PRIMARY KEY(hashtag, tweet_id),
  CONSTRAINT tweet_fk FOREIGN KEY (tweet_id) REFERENCES tweet(id) 
    ON DELETE CASCADE ON UPDATE RESTRICT
) ENGINE=ndbcluster;


CREATE TABLE mention (
  at_user varchar(20) CHARACTER SET UTF16LE,
  tweet_id bigint unsigned, 
  PRIMARY KEY (at_user, tweet_id),
  CONSTRAINT tweet_fk FOREIGN KEY (tweet_id) REFERENCES tweet(id) 
    ON DELETE CASCADE ON UPDATE RESTRICT  
) ENGINE=ndbcluster;


CREATE TABLE follow (
  follower varchar(20) CHARACTER SET UTF16LE,
  followed varchar(20) CHARACTER SET UTF16LE,
  PRIMARY KEY (follower, followed),
  INDEX reverse_idx (followed, follower),
  CONSTRAINT follower_fk FOREIGN KEY (follower) REFERENCES author(user_name)
    ON DELETE CASCADE ON UPDATE RESTRICT,
  CONSTRAINT followed_fk FOREIGN KEY (followed) REFERENCES author(user_name) 
    ON DELETE CASCADE ON UPDATE RESTRICT  
) ENGINE=ndbcluster;

