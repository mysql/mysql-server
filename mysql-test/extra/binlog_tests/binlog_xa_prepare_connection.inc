#
# This file initiate connections to run XA transactions up to
# their prepare.
# Connection name, transaction name and its content depends on
# supplied parameters.
#
# param $type        type of transaction
# param $index       index identifies the connection with those of type $type
# param $sql_init1   a query to execute once connection is established
# param $sql_init2   a query to execute once connection is established
# param $sql_doit    a query to execute inside transaction
#                    Note, the query may depend on tables created by caller
#

--connect (conn$index$type, 127.0.0.1,root,,test,$MASTER_MYPORT,)
if ($sql_init1)
{
  --eval $sql_init1
}
if ($sql_init2)
{
  --eval $sql_init2
}

--eval XA START   'trx$index$type'
if ($sql_doit)
{
  --disable_result_log
  --eval $sql_doit
  --enable_result_log
}
--eval XA END     'trx$index$type'
--eval XA PREPARE 'trx$index$type'
