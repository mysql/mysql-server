#
# Bug #30933728 INNODB FTS PHRASE SEARCH CRASH
#
# Case 1 : test phrase search with ngram.
# restart: --ngram_token_size=1
select @@ngram_token_size;
@@ngram_token_size
1
select @@innodb_ft_enable_stopword;
@@innodb_ft_enable_stopword
1
create table t1(
FTS_DOC_ID bigint unsigned auto_increment not null primary key,
content varchar(100) COLLATE utf8mb4_danish_ci
)DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_danish_ci;
create fulltext index fts_index on t1(content) with parser ngram;
insert into t1(content) values('María'), ('Maríb'), ('Maria'), ('Marib');
select * from t1 where match(content) against('+María' IN BOOLEAN MODE);
FTS_DOC_ID	content
1	María
select * from t1 where match(content) against('+Maríb' IN BOOLEAN MODE);
FTS_DOC_ID	content
2	Maríb
select * from t1 where match(content) against('+Maria' IN BOOLEAN MODE);
FTS_DOC_ID	content
1	María
3	Maria
select * from t1 where match(content) against('+Marib' IN BOOLEAN MODE);
FTS_DOC_ID	content
2	Maríb
4	Marib
drop table t1;
# Case 2: test with ngram without stopword
# restart: --ngram_token_size=1 --innodb_ft_enable_stopword=off
select @@ngram_token_size;
@@ngram_token_size
1
select @@innodb_ft_enable_stopword;
@@innodb_ft_enable_stopword
0
create table t1(
FTS_DOC_ID bigint unsigned auto_increment not null primary key,
content varchar(100) COLLATE utf8mb4_danish_ci
) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_danish_ci;
create fulltext index fts_index on t1(content) with parser ngram;
insert into t1(content) values('María'), ('Maríb'), ('Maria'), ('Marib');
select * from t1 where match(content) against('+María' IN BOOLEAN MODE);
FTS_DOC_ID	content
1	María
3	Maria
select * from t1 where match(content) against('+Maríb' IN BOOLEAN MODE);
FTS_DOC_ID	content
2	Maríb
4	Marib
select * from t1 where match(content) against('+Maria' IN BOOLEAN MODE);
FTS_DOC_ID	content
1	María
3	Maria
select * from t1 where match(content) against('+Marib' IN BOOLEAN MODE);
FTS_DOC_ID	content
2	Maríb
4	Marib
drop table t1;
# Case 3: test phrase search without ngram.
create table t1(
FTS_DOC_ID bigint unsigned auto_increment not null primary key,
content varchar(100) COLLATE utf8mb4_danish_ci
) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_danish_ci;
create fulltext index fts_index on t1(content);
insert into t1(content) values('María'), ('Maríb'), ('Maria'), ('Marib');
select * from t1 where match(content) against('+María' IN BOOLEAN MODE);
FTS_DOC_ID	content
1	María
3	Maria
select * from t1 where match(content) against('+Maríb' IN BOOLEAN MODE);
FTS_DOC_ID	content
2	Maríb
4	Marib
select * from t1 where match(content) against('+Maria' IN BOOLEAN MODE);
FTS_DOC_ID	content
1	María
3	Maria
select * from t1 where match(content) against('+Marib' IN BOOLEAN MODE);
FTS_DOC_ID	content
2	Maríb
4	Marib
drop table t1;
# restart:
