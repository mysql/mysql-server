-- @(#)14.sql	2.1.8.1
-- TPC-H/TPC-R Promotion Effect Query (Q14)
-- Functional Query Definition
-- Approved February 1998
:o
:b
:x
select
	100.00 * sum(
		decode(substr(p_type, 1, 5), 'PROMO',
			l_extendedprice*(1-l_discount),0))
	/ sum(l_extendedprice * (1 - l_discount)) as promo_revenue
from
	lineitem,
	part
where
	l_partkey = p_partkey
	and l_shipdate >= ':1'
	and l_shipdate < adddate(':1', 30);
:e
