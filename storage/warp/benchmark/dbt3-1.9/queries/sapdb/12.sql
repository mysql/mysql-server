-- @(#)12.sql	2.1.8.1
-- TPC-H/TPC-R Shipping Modes and Order Priority Query (Q12)
-- Functional Query Definition
-- Approved February 1998
:o
:b
:x
select
	l_shipmode,
	sum(decode(o_orderpriority, '1-URGENT', 1, '2-HIGH',1, 0)) as high_line_count,
	        sum(decode(o_orderpriority, '1-URGENT', 0, '2-HIGH',0, 1)) as low_line_count
from
	orders,
	lineitem
where
	o_orderkey = l_orderkey
	and l_shipmode in (':1', ':2')
	and l_commitdate < l_receiptdate
	and l_shipdate < l_commitdate
	and l_receiptdate >= ':3'
	and l_receiptdate < adddate(':3', 365)
group by
	l_shipmode
order by
	l_shipmode;
:e
