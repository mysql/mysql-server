###################### patchwork-check.inc #############################
#                                                                      #
#  Basic routine for the generation and execution of prepared and non  #
#  prepared SQL statements.                                            #
#                                                                      #
#  Purpose: Simplify the check of complex statements with different    #
#           sets of parameters (data type, value)                      #
#                                                                      #
########################################################################

#
# NOTE: PLEASE BE VERY CAREFULL, WHEN CHANGING OR USING ;-) THIS ROUTINE.
#
# Please be aware, that this routine
#  -  will be sourced by several test case files stored within the
#     directory 'mysql-test/t'. So every change here will affect
#     several test cases.
#  -  does not check its own prequisites
#  -  modifies the content and the data type of the 
#     uservariables @var_1 ... @var_<n>
#
# Please preserve the '__<name>_' naming of the the auxiliary variables.
# These names should avoid that a test case writer accidently creates a
# variable with the same name. 
#

# naming conventions:
# stmt_c_  --> statement with constants like "select 1 "
# stmt_uv_ --> statement with uservariables like "select @var_1 "
# stmt_ph_ --> prepared statement with placeholders like "select ? "


#
# Explanation how to use this routine by an example:
#
# Content of the caller script:
#   ....
#   set @stmt_part1= 'SELECT f1 + '
#   set @stmt_part2= ' from t1 where f2= ' ;
#   set @stmt_part3= '' ;
#   set @string_1= "1";       set @type_1= "BIGINT";
#   set @string_2= "-2.3E-4"; set @type_2= "DOUBLE";
#   set @max_var_number= 2;
#   --source include/patchwork-check.inc
#
#   # The next testing rounds could start with
#   set @string_1= "NULL";    set @type_1= "BIGINT";
#   set @string_2= "-2.3E-4"; set @type_2= "DOUBLE";
#   --source include/patchwork-check.inc
#
#   set @string_1= "1";       set @type_1= "BIGINT";
#   set @string_2= "NULL";    set @type_2= "LONGTEXT";
#   --source include/patchwork-check.inc
#
# Statements and uservariables to be produced and executed by this routine
#   1.  Statements with constants  
#   1.1 stmt1= SELECT f1 + 1 from t1 where f2= -2.3E-4 ;
#   1.2 stmt1 as prepared statement
#   2.  Statements with uservariables
#       @var_n should be of data type @type_n (if possible) and have the 
#       content @string_n .
#   2.1 stmt2=  SELECT f1 + @var_1 from t1 where f2= @var_2
#   2.2 stmt2 as prepared statement
#   3.  prepared statement with placeholders
#       prepare stmt1 from 'SELECT f1 + ? from t1 where f2= ?'
#       execute stmt1 using @var_1, @var_2 
#
#   Every prepared statement variant of the "patchwork" is 3 times executed.
#
#
# Please have also also a look into 
#   - t/tooltest.test  , which checks                  or
#   - t/ps_12func.test , which contains test cases using
# this routine.
#


##############
#
# Prerequisites:
#
#   The caller script must set the following uservariables:
#
#     The statement pieces: @stmt_part1, @stmt_part2, ... , @stmt_part<n>
#
#     The parameter values: @string_1, ... , @string_<n - 1>
#         The parameter value should fit to the data type !
#         UPPER(@stmt_part1) = 'NULL' will cause (SQL) NULL as content.
#
#     The parameter data types: @type_1, ... , @type_<n - 1>
#         valid types are: BIGINT, DOUBLE, LONGTEXT, LONGBLOB
#
#         Attention: All other type assignments will lead to a 
#                    uservariable of type LONGTEXT !!
#
#     The number of parameter values must be published via
#         set @max_var_number= <n - 1> ;
#         
# Attention: This routine does not perform any check of the content
#            of these variables.
#

##############
#
# How is intended uservariable generated:
#
#   Step 1: generate a uservariable of the intended type
#
#   UPPER(@type_<n>)     statement 
#     BIGINT             set @var_<n>= 0
#     DOUBLE'            set @var_<n>idx_= 0.0
#     LONGTEXT'          set @var_<n>= "INIT"
#     LONGBLOB'          set @var_<n>= CAST("INIT" AS BINARY)
#     <all other>        set @var_<n>= "INIT"
#
#   Step 2: assign the value to the uservariable
#
#   IF ( UPPER(@string_<n>) != 'NULL')
#      UPPER(@type_<n>) 
#          BIGINT        set @var_<n>= CEIL(@string_<n>)
#          DOUBLE        set @var_<n>= @string_<n> + 0.0
#          LONGTEXT      set @var_<n>= @string_<n>
#          LONGBLOB      set @var_<n>= CAST(@string_<n> AS BINARY)
#          <all other>   set @var_<n>= @string_<n>
#   ELSE
#          set @var_<n>= NULL 
#


#
# How to debug this routine if something goes wrong:
#
# 1.   Put the line '--disable_abort_on_error' into the caller script
#        --> There will be no abort of mysqltest, if a statement fails.
#            You will get a protocol (in most cases).
# 2.   Put the line 'set $__debug_= 1 ;' into the caller script .
#      The next call of patchwork-check.inc will print
#      the type and content of generated uservariables and statements.
# 3.   disable the '--disable_query_log' option some lines below
#
# and please be patient towards this routine, it is far away from being perfect.
#


# Suppress the majority of the huge output concerning the statement and 
# uservariable generation
--disable_query_log

let $__idx_= 1 ;
eval set @__stmt_c_=  @stmt_part_$__idx_ ;
# If the number of variables is greater 0, we need also 
# - the statement with uservariables (stmt_uv)           and
# - the prepared statement with placeholders (stmt_ph)   and
# - the execute for the prepared statement with placeholders (execute_stmt_ph)
let $__with_var_= `select @max_var_number > 0`;
while ($__with_var_)
{
  eval set @__stmt_uv_= @stmt_part_$__idx_ ;
  eval set @__stmt_ph_= @stmt_part_$__idx_ ;
  set @__execute_stmt_ph= 'execute __stmt_ph_ using ' ;
  let $__num_= `select @max_var_number`;
  while ($__num_)
  {
    ##### Generate the Uservariables
    eval set @__my_init_= CASE UPPER(@type_$__idx_) 
         WHEN 'BIGINT'   THEN 'set @var_$__idx_= 0'
         WHEN 'DOUBLE'   THEN 'set @var_$__idx_= 0.0'
         WHEN 'LONGTEXT' THEN 'set @var_$__idx_= "INIT"'
         WHEN 'LONGBLOB' THEN 'set @var_$__idx_= CAST("INIT" AS BINARY)'
         ELSE 'set @var_$__idx_= "INIT"' END;
    # select @__my_init_ as "@__my_init_ is: " ;
    let $__my_init_= `select @__my_init_`;
    eval $__my_init_ ;
  
    eval set @__my_init_= CASE UPPER(@type_$__idx_) 
    WHEN 'BIGINT' THEN 
      "set @var_$__idx_= IF(UPPER(@string_$__idx_)!='NULL',CEIL(@string_$__idx_),NULL)" 
    WHEN 'DOUBLE' THEN 
      "set @var_$__idx_= IF(UPPER(@string_$__idx_)!='NULL',@string_$__idx_ + 0.0,NULL)"
    WHEN 'LONGTEXT' THEN
      "set @var_$__idx_= IF(UPPER(@string_$__idx_)!='NULL',@string_$__idx_,NULL)"
    WHEN 'LONGBLOB' THEN
      "set @var_$__idx_= IF(UPPER(@string_$__idx_)!='NULL',CAST(@string_$__idx_ AS BINARY),NULL)"
    ELSE 
      "set @var_$__idx_= IF(UPPER(@string_$__idx_)!='NULL',@string_$__idx_,NULL)" END;
    let $__my_init_= `select @__my_init_`;
    eval $__my_init_ ;

    ##### concat the variable to the statements
    ## with Constants
    #    1. replace ugly NULLs like 'NuLl' with 'NULL' for better readability
    #    2. Strings to be inserted into the statement must be quoted
    eval set @__stmt_c_= concat(
             @__stmt_c_, 
             IF(UPPER(@string_$__idx_)='NULL','NULL',
                  IF(UPPER(@type_$__idx_)='LONGTEXT' or UPPER(@type_$__idx_)='LONGBLOB',
                               concat('''',@string_$__idx_,''''), @string_$__idx_
                      ))) ;
    ## with Uservariables
    eval set @__stmt_uv_= concat(@__stmt_uv_, '@var_$__idx_') ;
    ## with placeholders
    eval set @__stmt_ph_= concat(@__stmt_ph_, '?') ;
  
    ##### complete the execute for the prepared statement with placeholders
    eval set @__execute_stmt_ph= concat(@__execute_stmt_ph, '@var_$__idx_,') ;
  
    inc $__idx_ ;
    ##### concat the next part of the statement to the statements
    eval set @__stmt_c_=  concat(@__stmt_c_,  @stmt_part_$__idx_ );
    eval set @__stmt_uv_= concat(@__stmt_uv_, @stmt_part_$__idx_ );
    eval set @__stmt_ph_= concat(@__stmt_ph_, @stmt_part_$__idx_ );

    dec $__num_ ;
  }
  # @__execute_stmt_ph contains a trailing ',' which must be cut away
  set @__execute_stmt_ph= substr(@__execute_stmt_ph,1,length(@__execute_stmt_ph) - 1);
  dec $__with_var_ ;
}

while ($__debug_)
{
  ### Print debug informations for patchwork with variables
  let $__with_var_= `select @max_var_number > 0`;
  while ($__with_var_)
  {
    ### Print out the content of the statement variables
    eval  select "--------------------------------------"
          as "the content of the statement variables"
    union select concat('@__stmt_c_  is: ',@__stmt_c_) 
    union select concat('@__stmt_uv_ is: ',@__stmt_uv_)
    union select concat('@__stmt_ph_ is: ',@__stmt_ph_)
    union select concat('@__execute_stmt_ph is: ',@__execute_stmt_ph);


    ### Print out the content of the uservariables
    select '--------------------------------------'
          as "the content of the parameter variables";
    set @__parameter_= 'select ';
    let $__num_= `select @max_var_number`;
    let $__idx_= 1 ;
    while ($__num_)
    {
      eval select @type_$__idx_   as type,
                  @string_$__idx_ as string,
                  @var_$__idx_    as uservariable ;
      eval set @__parameter_= concat(@__parameter_, '@var_$__idx_ ,');
      inc $__idx_ ;

      dec $__num_ ;
    }
    # @__parameter_ contains a trailing ',' which must be cut away
    set @__parameter_= substr(@__parameter_,1,length(@__parameter_) - 1);
    let $__aux_= `select @__parameter_` ;
    eval $__aux_ ;


    ### Create a table from the uservariables and print out the column types
    let $__aux_= `select concat('CREATE TABLE t9 AS ',@__parameter_)` ;
    --disable_warnings
    drop table if exists t9;
    --enable_warnings
    eval $__aux_ ;
    show create table t9;
    drop table t9;

    dec $__with_var_ ;
  }
  ### Print debug informations for patchwork without variables
  ###       stmt_uv, stmt_ph, execute_stmt_ph and uservariables do NOT exist
  let $__with_var_= `select @max_var_number = 0`;
  while ($__with_var_)
  {
    ### Print out the content of the statement variables
    eval  select "--------------------------------------"
          as "the content of the statement variable"
    union select concat('@__stmt_c_  is: ',@__stmt_c_) ; 

    dec $__with_var_ ;
  }


  dec $__debug_ ;
}

## copy the statements and the execute into $variables
#  (__stmt_ph_ is not needed)
## + generate the prepared statements
--enable_query_log
let $__stmt_c_=    `select @__stmt_c_`;
eval prepare __stmt_c_  from @__stmt_c_ ;
let $__with_var_= `select @max_var_number > 0`;
while ($__with_var_)
{
  let $__stmt_uv_=   `select @__stmt_uv_`;
  eval prepare __stmt_uv_ from @__stmt_uv_ ;
  let $__execute_ph= `select @__execute_stmt_ph`;
  eval prepare __stmt_ph_ from @__stmt_ph_ ;
  dec $__with_var_ ;
}


##### The execution of all statements
## statement with Constants
eval $__stmt_c_ ;
## prepared statement with Constants
execute __stmt_c_ ;
# Try to detect if the prior executes damaged the parse tree by
# two additional executes .
execute __stmt_c_ ;
execute __stmt_c_ ;
let $__with_var_= `select @max_var_number > 0`;
while ($__with_var_)
{
  ## statement with Uservariables
  eval $__stmt_uv_ ;
  ## prepared statement with Uservariables
  execute __stmt_uv_ ;
  # Try to detect if the prior executes damaged the parse tree by
  # two additional executes .
  execute __stmt_uv_ ;
  execute __stmt_uv_ ;
  ## prepared statement with placeholders
  eval $__execute_ph ;
  # Try to detect if the prior executes damaged the parse tree by
  # two additional executes .
  eval $__execute_ph ;
  eval $__execute_ph ;

  dec $__with_var_ ;
}
