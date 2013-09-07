use test;

-- tables must be dropped in reverse order of dependencies

DROP TABLE if exists hashtag;
DROP TABLE if exists tweet;
DROP TABLE if exists author;
DROP TABLE if exists follow;


CREATE TABLE author (
  user_name varchar(20) not null primary key,
  full_name varchar(250),
  tweets int unsigned not null default 0,
  followers int unsigned not null default 0,
  following int unsigned not null default 0  
) ENGINE=ndbcluster;


CREATE TABLE tweet (
  id bigint unsigned auto_increment not null primary key,
  author varchar(20),
  message varchar(140),
  date_created timestamp,  
  KEY idx_btree_date_created (date_created),
  KEY idx_btree_author(author),
  CONSTRAINT author_fk FOREIGN KEY (author) REFERENCES author(user_name) 
    ON DELETE CASCADE ON UPDATE RESTRICT
) ENGINE=ndbcluster;


CREATE TABLE hashtag (
  hashtag varchar(20),
  tweet_id bigint unsigned,
  date_created timestamp,  
  author varchar(20),
  PRIMARY KEY(hashtag, tweet_id),
  CONSTRAINT tweet_fk FOREIGN KEY (tweet_id) REFERENCES tweet(id) 
    ON DELETE CASCADE ON UPDATE RESTRICT
) ENGINE=ndbcluster;


CREATE TABLE follow (
  source varchar(20),
  dest varchar(20),
  PRIMARY KEY (source, dest),
  INDEX reverse_idx (dest, source)
) ENGINE=ndbcluster;




