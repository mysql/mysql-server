################ include/ps_renew.inc #################
#                                                     #
#    renew the content of t1 and t9     #
#                                                     #
#######################################################

# truncate could not be used, because it is not supported
# in tables of type MERGE
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;

delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
    c10= 1, c11= 1, c12 = 1,
    c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
    c16= '11:11:11', c17= '2004',
    c18= 1, c19=true, c20= 'a', c21= '123456789a', 
    c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
    c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
    c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
    c10= 9, c11= 9, c12 = 9,
    c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
    c16= '11:11:11', c17= '2004',
    c18= 1, c19=false, c20= 'a', c21= '123456789a', 
    c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
    c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
    c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
