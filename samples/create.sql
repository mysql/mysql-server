use test;


CREATE TABLE  if not exists author (
  user_name varchar(20) not null primary key,
  full_name varchar(250)
) ENGINE=ndbcluster;


CREATE TABLE  if not exists tweet (
  id bigint unsigned auto_increment not null primary key,
  author varchar(20),
  message varchar(140),
  date_created timestamp,  
  KEY idx_btree_date_created (date_created),
  KEY idx_btree_author(author),
  CONSTRAINT author_fk FOREIGN KEY (author) REFERENCES author(user_name) 
    ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=ndbcluster;


CREATE TABLE if not exists hashtag (
  hashtag varchar(20),
  tweet_id bigint unsigned,
  date_created timestamp,  
  author varchar(20),
  PRIMARY KEY(hashtag, tweet_id),
  CONSTRAINT tweet_fk FOREIGN KEY (tweet_id) REFERENCES tweet(id) 
    ON DELETE RESTRICT ON UPDATE RESTRICT
) ENGINE=ndbcluster;


