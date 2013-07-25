mysqlbuild=mysql-5.5.30-tokudb-7.0.3-e-linux-x86_64
setup.mysql.bash --mysqlbuild=$mysqlbuild --install=0
if [ $? = 0 ] ; then
    run.sql.bench.bash --mysqlbuild=$mysqlbuild --commit=1
fi

mysqlbuild=mariadb-5.5.30-tokudb-7.0.3-e-linux-x86_64
setup.mysql.bash --mysqlbuild=$mysqlbuild --install=0
if [ $? = 0 ] ; then
    run.atc.ontime.bash --mysqlbuild=$mysqlbuild --commit=1 --engine=tokudb
fi

mysqlbuild=mariadb-5.5.30-tokudb-7.0.3-linux-x86_64
setup.mysql.bash --mysqlbuild=$mysqlbuild --install=0
if [ $? = 0 ] ; then
    run.sql.bench.bash --mysqlbuild=$mysqlbuild --commit=1
fi

mysqlbuild=mariadb-5.5.30-tokudb-7.0.3-e-linux-x86_64
setup.mysql.bash --mysqlbuild=$mysqlbuild --install=0
if [ $? = 0 ] ; then
    run.sql.bench.bash --mysqlbuild=$mysqlbuild --commit=1
fi

mysqlbuild=mariadb-5.5.30-tokudb-7.0.3-e-linux-x86_64
setup.mysql.bash --mysqlbuild=$mysqlbuild --install=0
if [ $? = 0 ] ; then
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=1
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=1 --tokudb_load_save_space=1
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=10
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=10 --tokudb_load_save_space=1
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=30
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=30 --tokudb_load_save_space=1
fi

mysqlbuild=mariadb-5.5.30-tokudb-7.0.3-e-linux-x86_64
setup.mysql.bash --mysqlbuild=$mysqlbuild --install=0
if [ $? = 0 ] ; then
    run.iibench.bash --mysqlbuild=$mysqlbuild --commit=1 --max_rows=1000000000 --insert_only=0
    run.iibench.bash --mysqlbuild=$mysqlbuild --commit=1 --max_rows=1000000000 --replace_into
    run.iibench.bash --mysqlbuild=$mysqlbuild --commit=1 --max_rows=1000000000 --insert_ignore
    run.iibench.bash --mysqlbuild=$mysqlbuild --commit=1 --max_rows=1000000000 --insert_only=1
fi

mysqlbuild=mariadb-5.5.30-tokudb-7.0.3-e-linux-x86_64
setup.mysql.bash --mysqlbuild=$mysqlbuild --install=0
if [ $? = 0 ] ; then
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=100 --compare=0
    run.tpch.bash --mysqlbuild=$mysqlbuild --commit=1 --SCALE=100 --compare=0 --tokudb_load_save_space=1
fi
