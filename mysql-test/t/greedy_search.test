
--source include/no_valgrind_without_big.inc

# This test is for a specific code path in the old optimizer. Skip it
# with the hypergraph optimizer.
--source include/not_hypergraph.inc

# To make sure we have stable costs, set memory io cost equal to disk io cost
UPDATE mysql.engine_cost
SET cost_value = 1.0
WHERE cost_name = 'memory_block_read_cost';
FLUSH OPTIMIZER_COSTS;

# Only new connections see the new cost constants
connect (con1, localhost, root,,);

--echo #
--echo # TEST 1
--echo # Greedy search iteration test for 16-way join: star schema
--echo #
--echo # Creation of 16 tables hidden
--echo #

--disable_result_log
--disable_query_log

let $brands=7;
let $models_pr_brand=5;
let $misc_properties_big=20;
let $misc_properties_small=10;
let $vehicles=100;

eval 
CREATE TABLE brands (
  id_pk int PRIMARY KEY, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$brands)
{
  inc $i;
  eval INSERT INTO brands VALUES ($i, $i, concat('brand',$i));
}

eval 
CREATE TABLE models (
  id_pk int PRIMARY KEY, 
  id_nokey int, 
  brand_id int, 
  name varchar(100), 
  INDEX(`brand_id`)
);

let $i=0;
let $cnt=0;
while ($i<$brands)
{
  inc $i;
  let $j=0;
  while ($j<$models_pr_brand)
  { 
    inc $cnt;
    inc $j;
    eval INSERT INTO models VALUES ($cnt, $cnt, $i, concat('brandmodel',$cnt));  
  }
}

eval 
CREATE TABLE subtypes (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO subtypes VALUES ($i, $i, concat('subtype',$i));
}

eval 
CREATE TABLE colors (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_big)
{
  inc $i;
  eval INSERT INTO colors VALUES ($i, $i, concat('color',$i));
}

eval 
CREATE TABLE heating (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO heating VALUES ($i, $i, concat('heating',$i));
}

eval 
CREATE TABLE windows (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO windows VALUES ($i, $i, concat('window',$i));
}

eval 
CREATE TABLE fuels (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO fuels VALUES ($i, $i, concat('fuel',$i));
}

eval 
CREATE TABLE transmissions (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO transmissions VALUES ($i, $i, concat('transmission',$i));
}

eval 
CREATE TABLE steerings (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO steerings VALUES ($i, $i, concat('steering',$i));
}

eval 
CREATE TABLE interiors (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);

let $i=0;
while ($i<$misc_properties_big)
{
  inc $i;
  eval INSERT INTO interiors VALUES ($i, $i, concat('interior',$i));
}

eval 
CREATE TABLE drives (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);
let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO drives VALUES ($i, $i, concat('drive',$i));
}

eval 
CREATE TABLE wheels (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);
let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO wheels VALUES ($i, $i, concat('wheel',$i));
}

eval 
CREATE TABLE engine (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);
let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO engine VALUES ($i, $i, concat('engine',$i));
}

eval 
CREATE TABLE price_ranges (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);
let $i=0;
while ($i<$misc_properties_small)
{
  inc $i;
  eval INSERT INTO price_ranges VALUES ($i, $i, concat('price',$i));
}

eval 
CREATE TABLE countries (
  id_pk int primary key, 
  id_nokey int, 
  name varchar(100)
);
let $i=0;
while ($i<$misc_properties_big)
{
  inc $i;
  eval INSERT INTO countries VALUES ($i, $i, concat('country',$i));
}

eval 
CREATE TABLE vehicles (
  id int primary key, 
  model_id int, 
  subtype_id int, 
  color_id int, 
  heating_id int, 
  window_id int, 
  fuel_id int, 
  transmission_id int, 
  steering_id int, 
  interior_id int, 
  drive_id int, 
  price_range_id int, 
  assembled_in int, 
  engine_id int, 
  wheels_id int
);


let $brands=7;
let $models_pr_brand=3;
let $misc_properties_big=20;
let $misc_properties_small=$misc_properties_big/2;

while ($i<$vehicles)
{
  inc $i;
  eval INSERT INTO vehicles VALUES ($i, $i%2, $i%3, $i%4, $i%5, $i%6, $i%7, 
                                    $i, $i%2, $i%3, $i%4, $i%5, $i%6, $i%7, 
                                    $i );
}

ANALYZE TABLE models, subtypes, colors, heating, windows, fuels, transmissions, steerings, interiors, drives, wheels, engine, price_ranges, countries, brands;
--enable_result_log
--enable_query_log

SET SESSION optimizer_search_depth = 25; 
# print_greedy_search_count will do it's own FLUSH STATUS after
# executing each query, but we need to reset counters before 
# the first query is executed as well.
FLUSH STATUS;

--echo #
--echo # 16-way join - all 15 fact tables joined on column with key
--echo #

let $query=
SELECT *
FROM vehicles
  JOIN models        ON vehicles.model_id        = models.id_pk
  JOIN subtypes      ON vehicles.subtype_id      = subtypes.id_pk
  JOIN colors        ON vehicles.color_id        = colors.id_pk
  JOIN heating       ON vehicles.heating_id      = heating.id_pk
  JOIN windows       ON vehicles.window_id       = windows.id_pk
  JOIN fuels         ON vehicles.fuel_id         = fuels.id_pk
  JOIN transmissions ON vehicles.transmission_id = transmissions.id_pk
  JOIN steerings     ON vehicles.steering_id     = steerings.id_pk
  JOIN interiors     ON vehicles.interior_id     = interiors.id_pk
  JOIN drives        ON vehicles.drive_id        = drives.id_pk
  JOIN wheels        ON vehicles.wheels_id       = wheels.id_pk
  JOIN engine        ON vehicles.engine_id       = engine.id_pk
  JOIN price_ranges  ON vehicles.price_range_id  = price_ranges.id_pk
  JOIN countries     ON vehicles.assembled_in    = countries.id_pk
  JOIN brands        ON models.brand_id          = brands.id_pk;
--source include/print_greedy_search_count.inc

--echo #
--echo # 16-way join - 10 fact tables joined on column with key and
--echo #                5 fact tables joined on column without key
--echo #

let $query=
SELECT *
FROM vehicles
  JOIN models        ON vehicles.model_id        = models.id_nokey
  JOIN subtypes      ON vehicles.subtype_id      = subtypes.id_pk
  JOIN colors        ON vehicles.color_id        = colors.id_pk
  JOIN heating       ON vehicles.heating_id      = heating.id_nokey
  JOIN windows       ON vehicles.window_id       = windows.id_pk
  JOIN fuels         ON vehicles.fuel_id         = fuels.id_pk
  JOIN transmissions ON vehicles.transmission_id = transmissions.id_nokey
  JOIN steerings     ON vehicles.steering_id     = steerings.id_pk
  JOIN interiors     ON vehicles.interior_id     = interiors.id_pk
  JOIN drives        ON vehicles.drive_id        = drives.id_pk
  JOIN wheels        ON vehicles.wheels_id       = wheels.id_nokey
  JOIN engine        ON vehicles.engine_id       = engine.id_pk
  JOIN price_ranges  ON vehicles.price_range_id  = price_ranges.id_pk
  JOIN countries     ON vehicles.assembled_in    = countries.id_pk
  JOIN brands        ON models.brand_id          = brands.id_nokey;
--source include/print_greedy_search_count.inc

select @@optimizer_search_depth;
select @@optimizer_prune_level;

DROP TABLE vehicles, models, subtypes, colors, heating, windows, 
           fuels, transmissions, steerings, interiors, drives, 
           price_ranges, countries, brands, wheels, engine;


--echo #
--echo # TEST 2
--echo # Greedy search iteration test for chain of tables
--echo #

--source include/greedy_search_load_tables.inc

# Explanation to the test
#
# A chain of tables is joined like this:
#    t1 JOIN t2 ON t1.<some_col>=t2.<some_col> JOIN t3 ON ...
#
# Different variants of table sizes and columns in the join conditions
# are tested. 
#
# The column names mean:
#   'pk'     - The column is primary key
#   'colidx' - The column is indexed
#   'col'    - The column is not indexed
#
# The table names mean:
#   tx_y     - table with x rows, y is simply used to get unique table names
#
# A comment explains each test. The notation used is
#    (...,tx_col_next):(ty_col_prev,...)
# which means that table x is joined with table y with join condition
# "ON tx.col_next = ty.col_prev" like this:
#    t1 JOIN t2 ON t1.col_next=t2.col_prev ...

--echo #
--echo # Chain test a:      colidx):(pk,colidx):(pk,colidx)
--echo #

let $query= SELECT * FROM t10_1;
let $i= 1;
while ($i < 8)
{
  let $query= $query JOIN t100_$i ON t10_$i.colidx = t100_$i.pk;
  let $j=$i;
  inc $j;
  let $query= $query JOIN t10_$j ON t100_$i.colidx = t10_$j.pk;
  inc $i;
}
--source include/print_greedy_search_count.inc

--echo #
--echo # Chain test b: (...,col):(colidx, col):(pk,col):(colidx,col):(pk,...)
--echo #
let $query= SELECT * FROM t10_1;
let $i= 1;
while ($i < 8)
{
  let $query= $query JOIN t100_$i ON t10_$i.col = t100_$i.colidx;
  let $j=$i;
  inc $j;
  let $query= $query JOIN t10_$j ON t100_$i.col = t10_$j.pk;
  inc $i;
}
--source include/print_greedy_search_count.inc

--echo #
--echo # Chain test c: (...,colidx):(col, pk):(col,colidx):(col,...)
--echo #
let $query= SELECT * FROM t10_1;
let $i= 1;
while ($i < 8)
{
  let $query= $query JOIN t100_$i ON t10_$i.colidx = t100_$i.col;
  let $j=$i;
  inc $j;
  let $query= $query JOIN t10_$j ON t100_$i.pk = t10_$j.col;
  inc $i;
}
--source include/print_greedy_search_count.inc

--echo #
--echo # Chain test d: (...,colidx):(pk, col):(pk,colidx):(pk,col):(pk,...)
--echo #
let $query= SELECT * FROM t10_1;
let $i= 1;
while ($i < 8)
{
  let $query= $query JOIN t100_$i ON t10_$i.colidx = t100_$i.pk;
  let $j=$i;
  inc $j;
  let $query= $query JOIN t10_$j ON t100_$i.col = t10_$j.pk;
  inc $i;
}
--source include/print_greedy_search_count.inc

--echo #
--echo # Cleanup after TEST 2
--echo #

--source include/greedy_search_drop_tables.inc

# Reset cost value to make sure it does not effect other tests
UPDATE mysql.engine_cost
SET cost_value = DEFAULT
WHERE cost_name = 'memory_block_read_cost';
FLUSH OPTIMIZER_COSTS;
