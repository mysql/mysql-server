let $restart_parameters = restart: --ngram_token_size=$ngram_token_size;
-- source include/restart_mysqld.inc

SELECT  @@ngram_token_size;

# New row inserted should be tokenized correctly by new ngram_token_size
INSERT INTO articles VALUES
  (5, '数据库管理系统','计算机是处理大量数据的理想工具');

-- echo # Some results are incorrect due to ngram_token_size mismatch

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据库' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据库管理' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据库管理系统' IN BOOLEAN MODE);

DROP INDEX ft_index ON articles;

DELETE FROM articles WHERE title = '数据库管理系统';

CREATE FULLTEXT INDEX ft_index ON articles(title, body) WITH PARSER ngram;

-- echo # Results are correct after rebuilding the index

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据库' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据库管理' IN BOOLEAN MODE);

SELECT * FROM articles WHERE
        MATCH(title, body) AGAINST('数据库管理系统' IN BOOLEAN MODE);
