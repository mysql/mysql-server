SHOW CREATE TABLE t_10;
SHOW CREATE TABLE t_100;
SHOW CREATE TABLE t_1000;
SHOW CREATE TABLE tp_r;
SHOW CREATE TABLE tsp_r;

--sorted_result
SELECT * FROM t_10;
--sorted_result
SELECT * FROM t_100;
--sorted_result
SELECT * FROM t_1000;

--sorted_result
SELECT * FROM tp_r;
--sorted_result
SELECT * FROM tp_r WHERE a< 10;
--sorted_result
SELECT * FROM tp_r WHERE a BETWEEN 11 AND 100;
--sorted_result
SELECT * FROM tp_r WHERE a BETWEEN 101 AND 200;
--sorted_result
SELECT * FROM tsp_r;

--sorted_result
SELECT * FROM tp_r PARTITION (p0);
--sorted_result
SELECT * FROM tp_r PARTITION (p1);
--sorted_result
SELECT * FROM tp_r PARTITION (p2);

--sorted_result
SELECT * FROM tsp_r PARTITION (p0);
--sorted_result
SELECT * FROM tsp_r PARTITION (p1);
--sorted_result
SELECT * FROM tsp_r PARTITION (p2);

--sorted_result
SELECT * FROM tsp_r PARTITION (sp00);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp01);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp02);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp03);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp04);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp10);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp11);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp12);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp13);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp14);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp20);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp21);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp22);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp23);
--sorted_result
SELECT * FROM tsp_r PARTITION (sp24);

--sorted_result
SELECT * FROM tp_rintvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_rintvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_rintvar PARTITION (p2);

--sorted_result
SELECT * FROM tp_rvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_rvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_rvar PARTITION (p2);

--sorted_result
SELECT * FROM tp_rintdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_rintdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_rintdate PARTITION (p2);

--sorted_result
SELECT * FROM tp_rdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_rdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_rdate PARTITION (p2);

--sorted_result
SELECT * FROM tsp_rvar PARTITION (p0);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (p1);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (p2);

--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp00);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp01);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp02);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp03);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp04);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp10);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp11);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp12);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp13);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp20);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp21);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp22);
--sorted_result
SELECT * FROM tsp_rvar PARTITION (sp23);

SELECT count(a) FROM tsp_rvar;

SELECT partition_name, subpartition_name FROM information_schema.partitions 
       WHERE table_name LIKE 'tsp_rvar1';
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p1);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p2);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp0);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp1);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp2);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp3);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p0sp4);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p1sp0);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p1sp1);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p1sp2);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p1sp3);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p1sp4);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p2sp0);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p2sp1);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p2sp2);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p2sp3);
--sorted_result
SELECT * FROM tsp_rvar1 PARTITION (p2sp4);

SELECT count(a) FROM tsp_rvar1;

SELECT partition_name, subpartition_name FROM information_schema.partitions 
      WHERE table_name LIKE 'tp_l';

SELECT * FROM tp_l PARTITION (p0);
--sorted_result
SELECT * FROM tp_l PARTITION (p1);
--sorted_result
SELECT * FROM tp_l PARTITION (p2);

SELECT count(a) FROM tp_l;

SELECT partition_name, subpartition_name FROM information_schema.partitions 
      WHERE table_name LIKE 'tsp_l';

--sorted_result
SELECT * FROM tsp_l PARTITION (p0);
--sorted_result
SELECT * FROM tsp_l PARTITION (p1);
--sorted_result
SELECT * FROM tsp_l PARTITION (p2);

--sorted_result
SELECT * FROM tsp_l PARTITION (sp00);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp01);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp02);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp03);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp04);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp10);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp11);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp12);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp13);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp20);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp21);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp22);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp23);
--sorted_result
SELECT * FROM tsp_l PARTITION (sp24);

SELECT count(a) FROM tsp_l;

--sorted_result
SELECT * FROM tp_k PARTITION (p0);
--sorted_result
SELECT * FROM tp_k PARTITION (p1);
--sorted_result
SELECT * FROM tp_k PARTITION (p2);

SELECT count(a) FROM tp_k;

--sorted_result
SELECT * FROM tp_kvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_kvar PARTITION (p2);

SELECT count(a) FROM tp_kvar;

--sorted_result
SELECT * FROM tp_kdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_kdate PARTITION (p2);

SELECT count(a) FROM tp_kdate;

--sorted_result
SELECT * FROM tp_h PARTITION (p0);
--sorted_result
SELECT * FROM tp_h PARTITION (p1);
--sorted_result
SELECT * FROM tp_h PARTITION (p2);

SELECT count(a) FROM tp_h;

--sorted_result
SELECT * FROM tp_hvar PARTITION (p0);
--sorted_result
SELECT * FROM tp_hvar PARTITION (p1);
--sorted_result
SELECT * FROM tp_hvar PARTITION (p2);

SELECT count(a) FROM tp_hvar;

--sorted_result
SELECT * FROM tp_hdate PARTITION (p0);
--sorted_result
SELECT * FROM tp_hdate PARTITION (p1);
--sorted_result
SELECT * FROM tp_hdate PARTITION (p2);

SELECT count(a) FROM tp_hdate;

