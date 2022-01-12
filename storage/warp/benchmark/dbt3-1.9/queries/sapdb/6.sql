-- @(#)6.sql	2.1.8.1
-- TPC-H/TPC-R Forecasting Revenue Change Query (Q6)
-- Functional Query Definition
-- Approved February 1998
:o
:b
:x
select
	sum(l_extendedprice * l_discount) as revenue
from
	lineitem
where
	l_shipdate >= ':1'
	and l_shipdate < adddate(':1', 365)
	and l_discount between :2 - 0.01 and :2 + 0.01
	and l_quantity < :3;
:e
