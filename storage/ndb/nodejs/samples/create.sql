use test;
create table if not exists tweet (
  id char(36) not null primary key,
  author varchar(20),
  message varchar(140),
  date_created timestamp,
  
  key idx_btree_date_created (date_created),
  key idx_btree_author(author)

  ) ENGINE=ndbcluster;
