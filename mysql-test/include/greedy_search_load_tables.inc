--disable_query_log
--disable_result_log

# tbl10 and tbl100 are only used to fill the tables used during testing
CREATE TABLE tbl10(
  k INT NOT NULL AUTO_INCREMENT,
  i INT,
  PRIMARY KEY(k)
);
INSERT INTO tbl10(i) VALUES (1),(2),(3),(4),(5),(6),(7),(8),(9),(0);

CREATE TABLE tbl100 LIKE tbl10;
INSERT INTO tbl100(i) SELECT x.i FROM tbl10 AS x, tbl10 AS y;

let $i= 1;
WHILE ($i < 10)
{
  --let $stmt= CREATE TABLE t10_$i (pk int PRIMARY KEY AUTO_INCREMENT, colidx int, col int, index (colidx))
  --eval $stmt;
  --let $stmt= INSERT INTO t10_$i (colidx, col) SELECT k, k FROM tbl10
  --eval $stmt;
  # Update index statistics
  ANALYZE TABLE t10_$i;
  
  --let $stmt= CREATE TABLE t100_$i (pk int PRIMARY KEY AUTO_INCREMENT, colidx int, col int, index (colidx))
  --eval $stmt;
  --let $stmt= INSERT INTO t100_$i (colidx, col) SELECT a.k, a.k FROM tbl10 a JOIN tbl10 b;
  --eval $stmt;
  # Update index statistics
  ANALYZE TABLE t10_$i;

  inc $i;
}  
--enable_result_log
--enable_query_log
