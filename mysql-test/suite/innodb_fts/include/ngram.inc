SET NAMES utf8;

-- echo # Create table : Test with row_type $table_row_format with char,varchar,text columns
EVAL CREATE TABLE articles_1 (
        seq INT ,
        title_char VARCHAR(200),
        title_varchar VARCHAR(300),
        title_text VARCHAR(300)
        ) charset latin1 ENGINE=InnoDB $table_row_format;

EVAL CREATE TABLE articles (
        seq INT AUTO_INCREMENT NOT NULL PRIMARY KEY,
        c1 VARCHAR(200) CHARACTER SET big5 COLLATE big5_chinese_ci,
        c2 CHAR(200) CHARACTER SET gbk COLLATE gbk_chinese_ci,
        c3 CHAR(200) CHARACTER SET utf8,
        j1 TEXT CHARACTER SET ujis COLLATE ujis_japanese_ci,
        j2 VARCHAR(200) CHARACTER SET sjis COLLATE sjis_japanese_ci,
        j3 VARCHAR(200) CHARACTER SET ujis,
        k1 CHAR(200) CHARACTER SET euckr COLLATE euckr_korean_ci,
        k2 CHAR(200) CHARACTER SET utf8,
        e1 CHAR(200) CHARACTER SET latin1,
        FULLTEXT KEY con1 (c1) WITH PARSER ngram,
        FULLTEXT KEY con2 (j1,j3) WITH PARSER ngram,
        FULLTEXT KEY con3 (k2) WITH PARSER ngram
        ) ENGINE=InnoDB $table_row_format;

EVAL CREATE TABLE 名字 (
        seq INT AUTO_INCREMENT NOT NULL PRIMARY KEY,
        c1 VARCHAR(200) CHARACTER SET big5 COLLATE big5_chinese_ci,
        c2 CHAR(200) CHARACTER SET gbk COLLATE gbk_chinese_ci,
        c3 CHAR(200) CHARACTER SET utf8,
        さようなら TEXT CHARACTER SET ujis COLLATE ujis_japanese_ci,
        j2 VARCHAR(200) CHARACTER SET sjis COLLATE sjis_japanese_ci,
        j3 VARCHAR(200) CHARACTER SET ujis,
        행운을빈다 CHAR(200) CHARACTER SET euckr COLLATE euckr_korean_ci,
        k2 CHAR(200) CHARACTER SET utf8,
        e1 CHAR(200) CHARACTER SET latin1,
        FULLTEXT KEY con1 (c1) WITH PARSER ngram,
        FULLTEXT KEY con2 (さようなら,j3) WITH PARSER ngram,
        FULLTEXT KEY con3 (k2) WITH PARSER ngram
        ) ENGINE=InnoDB $table_row_format;


SET SESSION innodb_ft_enable_stopword=0;
--echo # Case : run ngram on english text.
ALTER TABLE articles_1 ADD FULLTEXT INDEX (title_char) WITH PARSER ngram;
ALTER TABLE articles_1 ADD FULLTEXT INDEX (title_varchar) WITH PARSER ngram;
ALTER TABLE articles_1 ADD FULLTEXT INDEX (title_text) WITH PARSER ngram;
ALTER TABLE articles_1 ADD FULLTEXT INDEX (title_char,title_varchar) WITH PARSER ngram;
ALTER TABLE articles_1 ADD FULLTEXT INDEX (title_text,title_varchar) WITH PARSER ngram;
SHOW CREATE TABLE articles;
INSERT INTO articles_1 (seq,title_char,title_varchar,title_text) VALUES
  (1,'English is a West Germanic language','It was first spoken in early medieval England','most widely used language in the world'),
  (2,'spoken as a first language by the majority populations of several states','including the United Kingdom i*', ' and the United States, Canada, Australia, Ireland'),
  (3,'English arose in the Anglo-Saxon kingdoms','its from of England and what is now southeast Scotland','Following the extensive influence of Great Britain and the United Kingdom'),
  (5,'significant number of English words are constructed on the basis of roots from Latin', 'because Latin in some form was the lingua franca of the Christian Church','and of European intellectual life'),
  (7,'spread across states by  United Kingdom ~ England army','The Oxford English Dictionary liSts over 250,000 distinct words+','it became the dominant language in the United States, Canada, Australia and New Zealand');

-- disable_result_log
ANALYZE TABLE articles_1;
-- enable_result_log

# bigram search
SELECT * FROM articles_1 WHERE
	MATCH(title_char) AGAINST('En');
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('nd');
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('50');
SELECT * FROM articles_1 WHERE
	MATCH(title_text) AGAINST('se');
SELECT * FROM articles_1 WHERE
	MATCH(title_char,title_varchar) AGAINST('al');
SELECT * FROM articles_1 WHERE
	MATCH(title_text,title_varchar) AGAINST('er');

# try search for an entire word
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_char) AGAINST('West');
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('first');
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_text) AGAINST('United');
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_char,title_varchar) AGAINST('in');
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_text,title_varchar) AGAINST('St');

# boolean mode
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_char) AGAINST('En la' IN BOOLEAN MODE);
SELECT * FROM articles_1 WHERE
	MATCH(title_char) AGAINST('+En -la' IN BOOLEAN MODE);
SELECT * FROM articles_1 WHERE
	MATCH(title_char) AGAINST('-En +la' IN BOOLEAN MODE);
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_char) AGAINST('+En +la' IN BOOLEAN MODE);
SELECT * FROM articles_1 WHERE
	MATCH(title_char) AGAINST('-En -la' IN BOOLEAN MODE);

--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('in nc' IN BOOLEAN MODE);
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('+in -nc' IN BOOLEAN MODE);
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('+in <la' IN BOOLEAN MODE);
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('+la >la' IN BOOLEAN MODE);
--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar) AGAINST('~in la' IN BOOLEAN MODE);

--sorted_result
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar,title_text) AGAINST('Ca Ox' IN BOOLEAN MODE);
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar,title_text) AGAINST('s+ Ox' IN BOOLEAN MODE);
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar,title_text) AGAINST('s+ i*' IN BOOLEAN MODE);
SELECT * FROM articles_1 WHERE
	MATCH(title_varchar,title_text) AGAINST('"Un" "-i*"' IN BOOLEAN MODE);

# query expansion mode
--sorted_result
SELECT * FROM articles_1 WHERE
        MATCH(title_char) AGAINST('la'  WITH QUERY EXPANSION);
--sorted_result
SELECT * FROM articles_1 WHERE
        MATCH(title_varchar) AGAINST('po'  WITH QUERY EXPANSION);
--sorted_result
SELECT * FROM articles_1 WHERE
        MATCH(title_text) AGAINST('Ca'  WITH QUERY EXPANSION);
--sorted_result
SELECT * FROM articles_1 WHERE
        MATCH(title_char,title_varchar) AGAINST('Ca'  WITH QUERY EXPANSION);
--sorted_result
SELECT * FROM articles_1 WHERE
        MATCH(title_text,title_varchar) AGAINST('Ca'  WITH QUERY EXPANSION);

# proximity search - not supported yet
SELECT * FROM articles_1
        WHERE MATCH (title_text)
        AGAINST ('"Ca da"@2' IN BOOLEAN MODE);
SELECT * FROM articles_1
        WHERE MATCH (title_varchar,title_text)
        AGAINST ('"Ca da"@5' IN BOOLEAN MODE);

# FTS condition and Join in where clause
--sorted_result
SELECT * FROM articles_1 t1,articles_1 t2
        WHERE MATCH(t2.title_varchar,t2.title_text) AGAINST('Ca Ox' IN BOOLEAN MODE)
        AND t1.seq = t2.seq ;
SELECT COUNT(*) FROM articles_1 t1,articles_1 t2
        WHERE MATCH(t1.title_char) AGAINST('la')
        AND t1.seq = t2.seq
        AND MATCH(t2.title_varchar) AGAINST('-da' IN BOOLEAN MODE) ;

# Check non ngram FTS
# tescase create ngran and non ngram FTS index
ALTER TABLE articles_1 ADD FULLTEXT INDEX (title_char,title_text,title_varchar);
SELECT * FROM articles_1
        WHERE MATCH (title_char,title_text,title_varchar)
        AGAINST ('kingdom');
SELECT * FROM articles_1
        WHERE MATCH (title_char,title_text,title_varchar)
        AGAINST ('+"kingdom" -"States"' IN BOOLEAN MODE);
SELECT * FROM articles_1
        WHERE MATCH (title_char,title_text,title_varchar)
        AGAINST ('"United States"@2' IN BOOLEAN MODE);
--sorted_result
SELECT * FROM articles_1
        WHERE MATCH (title_char,title_text,title_varchar)
        AGAINST ('States' WITH QUERY EXPANSION);



SET SESSION innodb_ft_enable_stopword=1;
DROP TABLE articles_1;

-- echo # Test Case 2: CJK charset and collation on row_type
# This test covers additionally
# Create FTS index using ALTER
# Create multiple FTS index on table
# Simultanously Create multiple FTS index Chinese , Japanese , Korean column
INSERT INTO articles (c1,c2,c3,j1,j2,j3,k1,k2,e1) VALUES
   ('歡迎','你好','喂 ','日の長','言も言わずに','つやってみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row row1 xy'),
   ('你好嗎?','我幾好，你呢','好耐冇見','左右','左右される','☆右折⇔左折','제 이름은 Charles입니다.','운동을 좋아해요*','row row1 value'),
   ('救命啊!','火啊! 你好','生日快樂 ','雨季','雨が降りそう つやってみよう','雨雲','저분은 영어를 잘 합니다','저는 서울에서 살고 있습니다.','recording test');

-- disable_result_log
ANALYZE TABLE articles;
-- enable_result_log

# check default value ngram_token_size
SELECT @@ngram_token_size;

SELECT * FROM articles;
ALTER TABLE articles ADD FULLTEXT INDEX con4 (c3) WITH PARSER ngram;
SHOW CREATE TABLE articles;
ALTER TABLE articles ADD FULLTEXT INDEX con5 (c2) WITH PARSER ngram;
SHOW CREATE TABLE articles;
ALTER TABLE articles ADD FULLTEXT INDEX con6 (j2) WITH PARSER ngram;
SHOW CREATE TABLE articles;
ALTER TABLE articles ADD FULLTEXT INDEX con7 (j3) WITH PARSER ngram;
SHOW CREATE TABLE articles;
ALTER TABLE articles ADD FULLTEXT INDEX con8 (k1) WITH PARSER ngram;
SHOW CREATE TABLE articles;
ALTER TABLE articles ADD FULLTEXT INDEX con9 (e1) WITH PARSER ngram;
SHOW CREATE TABLE articles;

# natural language search
SELECT * FROM articles WHERE
	MATCH(c1) AGAINST('救命');
SELECT * FROM articles WHERE
	MATCH(c2) AGAINST('我幾');

SELECT * FROM articles WHERE
	MATCH(j1,j3) AGAINST('やっ');
SELECT * FROM articles WHERE
	MATCH(j1,j3) AGAINST('右さ');
SELECT * FROM articles WHERE
	MATCH(j3) AGAINST('右さ');

SELECT * FROM articles WHERE
	MATCH(k1) AGAINST('Ch');
SELECT * FROM articles WHERE
	MATCH(k2) AGAINST('니다');

SELECT * FROM articles WHERE
	MATCH(e1) AGAINST('row');


# boolean mode
SELECT * FROM articles WHERE
        MATCH(c1) AGAINST("+啊! " IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(c2) AGAINST("-我幾 -火啊" IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(c2) AGAINST("我幾 火啊" IN BOOLEAN MODE);
--sorted_result
SELECT * FROM articles WHERE
        MATCH(c2) AGAINST("<我幾 >火啊" IN BOOLEAN MODE);

INSERT INTO articles (c1,c2,c3,j1,j2,j3,k1,k2,e1) VALUES
       ('歡迎','你好','喂 ','日の長','言も言わずに','つやてみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row row1 xy');

-- disable_result_log
ANALYZE TABLE articles;
-- enable_result_log

SELECT * FROM articles WHERE
        MATCH(j1,j3) AGAINST("つや +(てみ)" IN BOOLEAN MODE);
DELETE FROM articles WHERE seq = 4;

-- disable_result_log
ANALYZE TABLE articles;
-- enable_result_log

SELECT * FROM articles WHERE MATCH(c1) AGAINST('+ ! ' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(j1,j3) AGAINST("右さ -の長" IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(k2) AGAINST('-Ch +해요');

SELECT * FROM articles WHERE
        MATCH(e1) AGAINST("row1 -row" IN BOOLEAN MODE) ;


# Query expansion mode
SELECT * FROM articles WHERE
        MATCH(c1) AGAINST('命啊' WITH QUERY EXPANSION);
SELECT * FROM articles WHERE
        MATCH(j2) AGAINST('やっそう' WITH QUERY EXPANSION);
SELECT * FROM articles WHERE
        MATCH(j1,j3) AGAINST('右さ の長' WITH QUERY EXPANSION);
SELECT * FROM articles WHERE
        MATCH(j3) AGAINST('の長' WITH QUERY EXPANSION);
#SELECT * FROM articles WHERE
#        MATCH(k2) AGAINST('국말' WITH QUERY EXPANSION);
--sorted_result
SELECT * FROM articles WHERE
        MATCH(e1) AGAINST('testing' WITH QUERY EXPANSION) ;



# Proximity search
SELECT * FROM articles WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(j2) AGAINST('"やっそう"@5' IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(j1,j3) AGAINST('"右さ -の長"@4' IN BOOLEAN MODE);
SELECT * FROM articles WHERE
        MATCH(k2) AGAINST('"국말 Ch"@2' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(e1) AGAINST('"row row1"@1' IN BOOLEAN MODE);


# Query with JOIN and FTS condition
SELECT COUNT(*) FROM articles t1,articles t2
        WHERE MATCH(t2.j1,t2.j3) AGAINST("右さ +の長" IN BOOLEAN MODE)
        AND t1.seq = t2.seq ;
SELECT COUNT(*) FROM articles t1,articles t2
        WHERE MATCH(t1.c1) AGAINST("命啊")
        AND t1.seq != t2.seq
        AND MATCH(t2.k2) AGAINST("국말" IN BOOLEAN MODE) ;
SELECT COUNT(*) FROM articles t1,articles t2
        WHERE MATCH(t1.c1) AGAINST("啊! " WITH QUERY EXPANSION)
        AND t1.seq = t2.seq
        AND MATCH(t2.k2) AGAINST("<국말" IN BOOLEAN MODE) ;


SELECT * FROM articles;

DROP TABLE articles;


-- echo # Test Case : CJK charset and collation with globalized table, column names
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('歡迎','你好','喂 ','日の長','言も言わずに','つやってみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row'),
   ('你好嗎?','我幾好，你呢','好耐冇見','左右','左右される','☆右折⇔左折','제 이름은 Charles입니다.','운동을 좋아해요*','row value'),
   ('救命啊!','火啊! 你好','生日快樂 ','雨季','雨が降りそう つやってみよう','雨雲','저분은 영어를 잘 합니다','저는 서울에서 살고 있습니다.','recording test');

-- disable_result_log
ANALYZE TABLE 名字;
-- enable_result_log

SELECT * FROM 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con4 (c3) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con5 (c2) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con6 (j2) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con7 (j3) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con8 (행운을빈다) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con9 (e1) WITH PARSER ngram;
SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('やっ');
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('제가');
SELECT * FROM 名字 WHERE
        MATCH(e1) AGAINST('row');
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST("니다 -제가" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(e1) AGAINST("row -row1" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('"降り やっ"@4' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('"분은 어를"@2' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(e1) AGAINST('"row row"@1' IN BOOLEAN MODE);

EVAL CREATE TABLE tmp1 $table_row_format AS SELECT * FROM 名字 WHERE 1=2;
DROP TABLE 名字;
RENAME TABLE tmp1 TO 名字;
SHOW CREATE TABLE 名字;


-- echo # Test Case : CJK charset and collation with  FTS + transactional statements
SELECT * FROM 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con1 (c1) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con2 (さようなら,j3) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con3 (k2) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con4 (c3) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con5 (c2) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con6 (j2) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con7 (j3) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con8 (행운을빈다) WITH PARSER ngram;
SHOW CREATE TABLE 名字;

START TRANSACTION;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('歡迎','你好','喂 ','日の長','言も言わずに','つやってみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row'),
   ('你好嗎?','我幾好，你呢','好耐冇見','左右','左右される','☆右折⇔左折','제 이름은 Charles입니다.','운동을 좋아해요*','row value'),
   ('救命啊!','火啊! 你好','生日快樂 ','雨季','雨が降りそう つやってみよう','雨雲','저분은 영어를 잘 합니다','저는 서울에서 살고 있습니다.','recording test');

SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('やっ');
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('제가');
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST("니다 -제가" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('"降り やっ"@4' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('"분은 어를"@2' IN BOOLEAN MODE);
COMMIT;

-- disable_result_log
ANALYZE TABLE 名字;
-- enable_result_log

SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('やっ');
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('제가');
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST("니다 -제가" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('"降り やっ"@4' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('"분은 어를"@2' IN BOOLEAN MODE);
DELETE FROM 名字;
START TRANSACTION;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('歡迎','你好','喂 ','日の長','言も言わずに','つやってみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row');
SAVEPOINT A;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('你好嗎?','我幾好，你呢','好耐冇見','左右','左右される','☆右折⇔左折','제 이름은 Charles입니다.','운동을 좋아해요*','row value');
SAVEPOINT B;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('救命啊!','火啊! 你好','生日快樂 ','雨季','雨が降りそう つやってみよう','雨雲','저분은 영어를 잘 합니다','저는 서울에서 살고 있습니다.','recording test');

SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('やっ');
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('제가');
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST("니다 -제가" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('"降り やっ"@4' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('"분은 어를"@2' IN BOOLEAN MODE);
ROLLBACK TO B;
COMMIT;

SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('啊! ');
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('やっ');
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('제가');
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST("니다 -제가" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('"降り やっ"@4' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('"분은 어를"@2' IN BOOLEAN MODE);

DELETE FROM 名字;
START TRANSACTION;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('歡迎','你好','喂 ','日の長','言も言わずに','つやってみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row');
SAVEPOINT A;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('你好嗎?','我幾好，你呢','好耐冇見','左右','左右される','☆右折⇔左折','제 이름은 Charles입니다.','운동을 좋아해요*','row value');
SAVEPOINT B;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('救命啊!','火啊! 你好','生日快樂 ','雨季','雨が降りそう つやってみよう','雨雲','저분은 영어를 잘 합니다','저는 서울에서 살고 있습니다.','recording test');
ROLLBACK;
COMMIT;

-- disable_result_log
ANALYZE TABLE 名字;
-- enable_result_log

SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('やっ');
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('제가');
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST("니다 -제가" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('"降り やっ"@4' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('"분은 어를"@2' IN BOOLEAN MODE);

EVAL CREATE TABLE tmp1 $table_row_format AS SELECT * FROM 名字 WHERE 1=2;
DROP TABLE 名字;
RENAME TABLE tmp1 TO 名字;
SHOW CREATE TABLE 名字;

ALTER TABLE 名字 ADD FULLTEXT INDEX con1 (c1) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con2 (さようなら,j3) WITH PARSER ngram;
SHOW CREATE TABLE 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con3 (k2) WITH PARSER ngram;
SHOW CREATE TABLE 名字;

SELECT * FROM 名字;
START TRANSACTION;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('歡迎','你好','喂 ','日の長','言も言わずに','つやってみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row');
SAVEPOINT A;
ALTER TABLE 名字 ADD FULLTEXT INDEX con5 (c2) WITH PARSER ngram;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('你好嗎?','我幾好，你呢','好耐冇見','左右','左右される','☆右折⇔左折','제 이름은 Charles입니다.','운동을 좋아해요*','row value');
ALTER TABLE 名字 ADD FULLTEXT INDEX con4 (c3) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con6 (j2) WITH PARSER ngram;
SAVEPOINT B;
INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('救命啊!','火啊! 你好','生日快樂 ','雨季','雨が降りそう つやってみよう','雨雲','저분은 영어를 잘 합니다','저는 서울에서 살고 있습니다.','recording test');
ALTER TABLE 名字 ADD FULLTEXT INDEX con7 (j3) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con8 (행운을빈다) WITH PARSER ngram;
COMMIT;

-- disable_result_log
ANALYZE TABLE 名字;
-- enable_result_log

SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('やっ');
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('제가');
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST("-我幾 火啊" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j2) AGAINST("やっ-(そう)" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(j3) AGAINST(">の長" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST("니다 -제가" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(c2) AGAINST('"我幾 火啊"@3' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('"降り やっ"@4' IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(행운을빈다) AGAINST('"분은 어를"@2' IN BOOLEAN MODE);

EVAL CREATE TABLE tmp1 $table_row_format AS SELECT * FROM 名字 WHERE 1=2;
DROP TABLE 名字;
RENAME TABLE tmp1 TO 名字;
SHOW CREATE TABLE 名字;

-- echo # Test Case : CJK charset and collation , FTS query in condition of update/delete

SELECT * FROM 名字;
ALTER TABLE 名字 ADD FULLTEXT INDEX con1 (c1) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con2 (さようなら,j3) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con3 (k2) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con4 (c3) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con5 (c2) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con6 (j2) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con7 (j3) WITH PARSER ngram;
ALTER TABLE 名字 ADD FULLTEXT INDEX con8 (행운을빈다) WITH PARSER ngram;

INSERT INTO 名字 (c1,c2,c3,さようなら,j2,j3,행운을빈다,k2,e1) VALUES
   ('歡迎','你好','喂 ','日の長','言も言わずに','つやってみよう','제가 미국인입니다.','한국말을 공부합니다%','testing row'),
   ('你好嗎?','我幾好，你呢','好耐冇見','左右','左右される','☆右折⇔左折','제 이름은 Charles입니다.','운동을 좋아해요*','row value'),
   ('救命啊!','火啊! 你好','生日快樂 ','雨季','雨が降りそう つやってみよう','雨雲','저분은 영어를 잘 합니다','저는 서울에서 살고 있습니다.','recording test');

-- disable_result_log
ANALYZE TABLE 名字;
-- enable_result_log

SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
UPDATE 名字 SET c1 = '你好嗎?' WHERE MATCH(c2) AGAINST('啊!');
SELECT * FROM 名字 WHERE
        MATCH(c1) AGAINST('命啊');
DELETE FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST("てみ -雨が" IN BOOLEAN MODE);
SELECT * FROM 名字 WHERE
        MATCH(さようなら,j3) AGAINST('てみ');
SELECT * FROM 名字;
SELECT * FROM 名字 WHERE
        MATCH (행운을빈다) AGAINST('니다');
DELETE FROM 名字 WHERE
        MATCH (행운을빈다) AGAINST('니다');
SELECT * FROM 名字;

DROP TABLE 名字;
