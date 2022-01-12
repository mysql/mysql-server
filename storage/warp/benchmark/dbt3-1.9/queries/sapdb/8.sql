-- @(#)8.sql	2.1.8.1
-- TPC-H/TPC-R National Market Share Query (Q8)
-- Functional Query Definition
-- Approved February 1998
:o
:b
:x
select
	o_year,
	sum(decode (nation, 'FRANCE', volume, 0))
	/ sum(volume) as mkt_share
from
	(
		select
			substr(o_orderdate, 1, 4) as o_year,
			l_extendedprice * (1 - l_discount) as volume,
			n2.n_name as nation
		from
			part,
			supplier,
			lineitem,
			orders,
			customer,
			nation n1,
			nation n2,
			region
		where
			p_partkey = l_partkey
			and s_suppkey = l_suppkey
			and l_orderkey = o_orderkey
			and o_custkey = c_custkey
			and c_nationkey = n1.n_nationkey
			and n1.n_regionkey = r_regionkey
			and r_name = ':2'
			and s_nationkey = n2.n_nationkey
			and o_orderdate between '1995-01-01' and '1996-12-31'
			and p_type = ':3'
	) 
group by
	o_year
order by
	o_year;
:e
