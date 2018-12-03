# This table scheme is intended to have these properties:
# 1. it has a virtual column
# 2. the virtual column does not depend on one of columns
# In our case v1 does not depend on c2, and we will use this to
# expose a bug in handling secondary indexes on virtual columns,
# by UPDATEing c2, and causing the algorithm to believe it caused
# change to v1, while it obviously didn't.

CREATE TABLE t1(
  id INT NOT NULL,
  c1 INT NOT NULL,
  c2 INT NOT NULL,
  v1 INT AS (c1) VIRTUAL,
  PRIMARY KEY (id DESC),
  UNIQUE KEY(v1)
) Engine=InnoDB;

INSERT INTO t1 (id,c1,c2) VALUES (0,0,0),(1,1,1),(3,3,3);

# lock_sec_rec_some_has_impl has some heuristics which try to avoid
# invoking costly algorithm, by performing some easy checks first,
# one of which is to compare page_get_max_trx_id(page) with
# trx_rw_min_trx_id(). To help us pass through this check, we keep
# create a rw-transaction from `view_keeper` and keep it open, and then we
# also modify the secondary index page from `default`.
# Keep trx_rw_min_trx_id() low:
--connect (view_keeper, localhost, root,,)
  BEGIN;
  UPDATE t1 SET c2=13 WHERE id = 3;
# Make page_get_max_trx_id(block->frame) updated:
--connection default
  INSERT INTO t1 (id,c1,c2) VALUES (4,4,4);