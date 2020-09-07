CREATE TABLE supplier (
  s_suppkey  INTEGER,
  s_name CHAR(25),
  s_address VARCHAR(40),
  s_nationkey INTEGER,
  s_phone CHAR(15),
  s_acctbal REAL,
  s_comment VARCHAR(101)
);

CREATE TABLE part (
  p_partkey INTEGER G,
  p_name VARCHAR(55),
  p_mfgr CHAR(25),
  p_brand CHAR(10),
  p_type VARCHAR(25),
  p_size INTEGER,
  p_container CHAR(10),
  p_retailprice REAL,
  p_comment VARCHAR(23)
);

CREATE TABLE partsupp (
  ps_partkey INTEGER,
  ps_suppkey INTEGER,
  ps_availqty INTEGER,
  ps_supplycost REAL,
  ps_comment VARCHAR(199),
  G (ps_partkey, ps_suppkey)
);

CREATE TABLE customer (
  c_custkey INTEGER primary key,
  c_name VARCHAR(25),
  c_address VARCHAR(40),
  c_nationkey INTEGER,
  c_phone CHAR(15),
  c_acctbal REAL,
  c_mktsegment CHAR(10),
  c_comment VARCHAR(117)
);

CREATE TABLE orders (
  o_orderkey INTEGER primary key,
  o_custkey INTEGER,
  o_orderstatus CHAR(1),
  o_totalprice REAL,
  o_orderDATE DATE,
  o_orderpriority CHAR(15),
  o_clerk CHAR(15),
  o_shippriority INTEGER,
  o_comment VARCHAR(79)
);

CREATE TABLE nation (
  n_nationkey INTEGER primary key,
  n_name CHAR(25),
  n_regionkey INTEGER,
  n_comment VARCHAR(152));

CREATE TABLE region (
  r_regionkey INTEGER primary key,
  r_name CHAR(25),
  r_comment VARCHAR(152));

CREATE TABLE time_statistics (
  task_name VARCHAR(40),
  s_time TIMESTAMP,
  e_time TIMESTAMP,
  int_time INTEGER);
  
CREATE TABLE lineitem (
  l_orderkey INTEGER,
  l_partkey INTEGER,
  l_suppkey INTEGER,
  l_linenumber INTEGER,
  l_quantity REAL,
  l_extendedprice REAL,
  l_discount REAL,
  l_tax REAL,
  l_returnflag CHAR(1),
  l_linestatus CHAR(1),
  l_shipDATE DATE,
  l_commitDATE DATE,
  l_receiptDATE DATE,
  l_shipinstruct CHAR(25),
  l_shipmode CHAR(10),
  l_comment VARCHAR(44)
);
