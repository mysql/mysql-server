-- @(#)19.sql	2.1.8.1
-- TPC-H/TPC-R Discounted Revenue Query (Q19)
-- Functional Query Definition
-- Approved February 1998
:x
:o
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
		p_brand = 'Brand#24'
		and p_container in ('SM CASE', 'SM BOX', 'SM PACK', 'SM PKG')
		and l_quantity >=10  and l_quantity <= 10 + 10
		and p_size between 1 and 5
	 )
	or
	 (
		p_brand = 'Brand#32'
		and p_container in ('MED BAG', 'MED BOX', 'MED PKG', 'MED PACK')
		and l_quantity >= 11 and l_quantity <= 11 + 10 
		and p_size between 1 and 10
	 )
	or
	 (
		p_brand = 'Brand#33'
		and p_container in ('LG CASE', 'LG BOX', 'LG PACK', 'LG PKG')
		and l_quantity >= 24 and l_quantity <= 24 + 10
		and p_size between 1 and 15
	 )
        );
:n -1
