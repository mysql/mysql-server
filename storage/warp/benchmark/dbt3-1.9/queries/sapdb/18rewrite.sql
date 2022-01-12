-- @(#)18.sql	2.1.8.1
-- TPC-H/TPC-R Large Volume Customer Query (Q18)
-- Function Query Definition
-- Approved February 1998
create table TEMP.l_orderkey_sumquantity(
	l_orderkey fixed(10),
	sumquantity fixed(12,2))

insert into TEMP.l_orderkey_sumquantity (
	select l_orderkey, sum(l_quantity) 
	from lineitem group by l_orderkey)
:x
:o
select
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice,
	sum(l_quantity)
from
	customer,
	orders,
	lineitem
where
	o_orderkey in (
		select
			l_orderkey
		from
			TEMP.l_orderkey_sumquantity
		where
			sumquantity > 313
	)
	and c_custkey = o_custkey
	and o_orderkey = l_orderkey
group by
	c_name,
	c_custkey,
	o_orderkey,
	o_orderdate,
	o_totalprice
order by
	o_totalprice desc,
	o_orderdate;
:n 100
