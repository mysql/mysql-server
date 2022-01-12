-- @(#)19.sql	2.1.8.1
-- TPC-H/TPC-R Discounted Revenue Query (Q19)
-- Functional Query Definition
-- Approved February 1998
:o
:b
:x
select
	sum(l_extendedprice* (1 - l_discount)) as revenue
from
	lineitem,
	part
where
	p_partkey = l_partkey
	and l_shipmode in ('AIR', 'AIR REG')
	and l_shipinstruct = 'DELIVER IN PERSON'
	and
	(
	 (
		p_brand = ':1'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >= :4 and l_quantity <= :4+10
		and p_size between 1 and 5
	 )
	 or
	 (
		p_brand = ':2'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= :5 and l_quantity <= :5+10
		and p_size between 1 and 10
	  )
	  or
	  (
		p_brand = ':3'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= :6 and l_quantity <= :6+10
		and p_size between 1 and 15
	  )
	);
:e
