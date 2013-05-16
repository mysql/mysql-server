# Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

################################################################################
# outer_join.yy
# Purpose:  Random Query Generator grammar for testing larger (6 - 10 tables) JOINs
# Tuning:   Please tweak the rule table_ref ratio of table:join for larger joins
#           NOTE:  be aware that larger (15-20 tables) queries can take far too 
#                  long to run to be of much interest for fast, automated testing
#
# Notes:    This grammar is designed to be used with gendata=conf/outer_join.zz
#           It can be altered, but one will likely need field names
#           Additionally, it is not recommended to use the standard RQG-produced
#           tables as they way we pick tables can result in the use of
#           several large tables that will bog down a generated query
#           
#           Please rely largely on the _portable variant of this grammar if
#           doing 3-way comparisons as it has altered code that will produce
#           more standards-compliant queries for use with other DBMS's
#  
#           We keep the grammar here as it is in order to also test certain
#           MySQL-specific syntax variants.
################################################################################

query:
   { @nonaggregates = (); @aggregates = (); $tables = 0; $ext = ""; $fields = 0; @outer_tables = (); $stack->push(); "" }
   query_expr order_by_clause
   { $stack->pop(undef) }
 ;


###############################################
## Ref: ISO 9075, Chap 6.3 <table reference> ##
###############################################
################################################################################
# recommend 4 tables : 1 joins for smaller queries, 3:1 for larger ones
################################################################################
table_ref:
   table | table | table | table
 | joined_table
#| derived_table      # See note where 'derived_table:' is defined.
 ;

##
# There are two problems with subqry as derived_tables which are the reason for not
# allowing them in this grammar:
#
# 1. Name resolving of outer referrence columns seems to be broken.
# 2. Create correct references to columns from the derived tables are complicated within RQG.
#    It might be doable with logic similar to 'lookahead_for_table_alias' in order, 
#    but I want invest time in this right now, and probably never.
#
# Testcoverage of the different flavours of subqueries are assumed to be sufficient
# throught scalar_subqry and IN/EXISTS
##
derived_table_unused:
   { $stack->push(); undef }
   table_subquery 
   {  my $x = " AS table".++$tables.$ext; my @s=($x); $stack->pop(\@s); $x }
 ;


##############################################
## Ref: ISO 9075, Chap 6.x <value_expr_xxx> ##
##############################################

value_expr:
   int_value_expr
 | char_value_expr
 ;

int_value_expr:
   _digit
 | int_column
#| int_aggregate
 | int_value_expr + int_value_expr
 | int_value_expr - int_value_expr
 | CHAR_LENGTH(char_value_expr)
 | (int_value_expr)
#| int_scalar_subquery
 ;

char_value_expr:
   char_column
 | {"'".$prng->string(4)."'"}
#| {"'"}_letter{"'"}
#| char_aggregate
#| UPPER(char_value_expr)
#| LOWER(char_value_expr)
#| TRIM(char_value_expr)
 | (char_value_expr)
#| char_scalar_subquery
 ;

char_string_literal:
   {"'".$prng->string(4)."'"}
 ;

int_column:
   table_alias.int_field_name
 ;

char_column:
   table_alias.char_field_name
 ;

#######################################################
## Ref: ISO 9075, Chap 7.2 <table value constructor> ##
#######################################################
table_value_constr:
   VALUES table_value_list
;

table_value_list:
   (1, 2, 3)
 ;

################################################
## Ref: ISO 9075, Chap 7.3 <table expression> ##
################################################
table_expr:
   from_clause where_clause group_by_clause having_clause
 ;

###########################################
## Ref: ISO 9075, Chap 7.4 <from clause> ##
###########################################
from_clause:
   FROM table_list
   { scalar(@tables) == $table_refs or die "Table lookahead predicted ".scalar(@tables)." tables, found: ".$tables."\n"; undef }
 ;

####
# <table referrence> has been inlined into <table list>, and decomposed
#  in order to increase frequency of:
#  - <table>, <table>, <table>...
#  - single <joined tables>
#  - single <table subquery>
# Particularly we want to avoid to many <joined table>, <joined table> ....
# being produced, they are allowed though.
# Tweak the rule 'table_ref_list' to adjust this.
####
table_list:
   table_ref_list
#| table_only_list
 ;

outer_join_pattern_unused:
   { $stack->push() }      
   table_ref { $stack->set("left",$stack->get("result")); }
   outer_join_type JOIN table_ref join_spec
   { my $left = $stack->get("left");  my $right = $stack->get("result"); my @n = (); push(@n,@$right); push(@n,@$left); $stack->pop(\@n); return undef }
 ;

table_only_list:
   table
 | table, table_only_list

#  { $stack->push() }      
#  table, { $stack->set("left",$stack->get("result")); }
#  table_only_list
 ;

table_ref_list:
   table
 | composite_table_ref
 | composite_table_ref
 | composite_table_ref
 | composite_table_ref
 | composite_table_ref
#| composite_table_ref, table_list
 ;

composite_table_ref:
   joined_table
#| derived_table      # See note where 'derived_table:' is defined.
 ;

table:
   # We use the "AS table" bit here so we can have unique aliases if we use the same table many times
   { $stack->push(); my $x = $prng->arrayElement($executors->[0]->tables())." AS table".++$tables.$ext; my @s=($x); $stack->pop(\@s); $x }
 ;

lookahead_for_table_alias:
   { @table_refs = (); my $tmp = join('', @sentence); my $lookahead = substr($tmp,index($tmp,$_)+length($_)); while ($lookahead =~ s/[(](?!SELECT)([^()]*)[)]/$1/g or $lookahead =~ s/[(]SELECT[^()]*[)]/<subqry>/g or $lookahead =~ s/(^[^(()]*)[)].*/$1/) {}; my @tab=(1..($lookahead =~ s/AS table//g)); foreach $id (@tab) { push (@table_refs,"table".$id.$ext) }; "" }
 ;

# NOTE can't make use of 'existing_table_item' as <table_expr> has not been produced yet
table_alias:
   # If there are 'outer tables': choose either inner or outer table_ref
   { $prng->arrayElement(\@table_refs) }
 | { scalar(@outer_tables) > 0 ? $prng->arrayElement(\@outer_tables) : $prng->arrayElement(\@table_refs) }
 ;

outer_table_unused:
   { $prng->arrayElement(\@outer_tables) }
 ;


############################################
## Ref: ISO 9075, Chap 7.5 <joined_table> ##
############################################
joined_table:
   qualified_join
 | qualified_join
 | qualified_join
 | qualified_join
 | qualified_join
 | qualified_join
#| cross_join
 | (joined_table)
 ;

qualified_join:
   inner_join
 | outer_join
 ;

cross_join:
   table_ref CROSS JOIN table_ref
   # Stupid MySQL extension where missing 'cross' join is determined based on no join_condition  
 | table_ref INNER JOIN table_ref
 | table_ref JOIN table_ref
 ;

# MySQL has the (stupid) cross_join extension (above) where it actually is the presence of the join_cond which
# determines if the join is a cross- or inner-join. This creates ambigous parser precedence rules which requires
# us to enclose right side argument in paranthesis - Else the 'outer_join:' rule is more in accordance
# with the ISO 9075 standards.
inner_join:
   { $stack->push() }      
   table_ref { $stack->set("left",$stack->get("result")); }
   inner_join_type JOIN (table_ref) join_spec
   { my $left = $stack->get("left");  my $right = $stack->get("result"); my @n = (); push(@n,@$right); push(@n,@$left); $stack->pop(\@n); return undef }
 ;

outer_join:
   { $stack->push() }      
   table_ref { $stack->set("left",$stack->get("result")); }
   outer_join_type JOIN table_ref join_spec
   { my $left = $stack->get("left");  my $right = $stack->get("result"); my @n = (); push(@n,@$right); push(@n,@$left); $stack->pop(\@n); return undef }
 ;

join_spec:
   ON join_condition
#| USING (pk)         # Hard to handle, need to refer a avail common column from left / right
 ;

inner_join_type:
   # Unspecified -> <join_type> implies 'INNER'
 | INNER
 ;

outer_join_type:
   LEFT outer
 | RIGHT outer
 ;

outer:
 | OUTER
 ;

# Prefer int_condition as char_condition will hardly ever match. 
join_condition:
   int_condition
 | int_condition
 | int_condition
 | int_condition
 | int_condition
 | int_condition
 | char_condition
 | other_condition
 ;

int_condition: 
   existing_left_table.int_field_name = existing_right_table.int_field_name     # General rule
 | existing_left_table.int_indexed = existing_right_table.int_indexed           # Want more joins on indexed field
 | int_multi_conditions
 ;

# Most of these join conditions crafted to match specific unique indexes
int_multi_conditions:
   # ix1(col_int,col_int_unique)
   existing_left_table.col_int = existing_right_table.col_int AND
   existing_left_table.col_int_unique = existing_right_table.col_int_unique
 |
   # ix2((col_int_key,col_int_unique))
   existing_left_table.col_int_key = existing_right_table.col_int_key AND
   existing_left_table.col_int_unique = existing_right_table.col_int_unique
 |
   # Variant of ix2() above
   existing_left_table.col_int_key = existing_right_table.int_field_name AND
   existing_left_table.col_int_unique = existing_right_table.int_field_name
 |
   # ix3(col_int,col_int_key,col_int_unique)
   existing_left_table.col_int = existing_right_table.col_int AND
   existing_left_table.col_int_key = existing_right_table.col_int_key AND
   existing_left_table.col_int_unique = existing_right_table.col_int_unique
#|
#  int_condition AND int_condition
 ;

char_condition:
   existing_left_table.char_field_name = existing_right_table.char_field_name   # General rule
 | existing_left_table.char_indexed = existing_right_table.char_indexed         # Want more joins on indexed field
 | char_multi_conditions
 ;

char_multi_conditions:
   # ix1(col_char_16,col_char_16_unique)
   existing_left_table.col_char_16 = existing_right_table.col_char_16 AND
   existing_left_table.col_char_16_unique = existing_right_table.col_char_16_unique
 |
   # ix2(col_varchar_256,col_char_16_unique)
   existing_left_table.col_varchar_256 = existing_right_table.col_varchar_256 AND
   existing_left_table.col_varchar_10_unique = existing_right_table.col_varchar_10_unique
#|
#  char_condition AND char_condition
 ;

other_condition:
   existing_left_table.col_int comparison_operator existing_right_table.col_int
 | existing_left_table.col_int IS not NULL
 | existing_right_table.col_int IS not NULL
 | existing_left_table.col_int not IN (number_list)
 | existing_right_table.col_int not IN (number_list)
 | join_condition and_or join_condition
 | not (join_condition) is_truth_value
 ;

existing_left_table:
   { my $left = $stack->get("left"); my %s=map{$_=>1} @$left; my @r=(keys %s); my $table_string = $prng->arrayElement(\@r); my @table_array = split(/AS/, $table_string); $table_array[1] }
 ;

existing_right_table:
   { my $right = $stack->get("result"); my %s=map{$_=>1} @$right; my @r=(keys %s); my $table_string = $prng->arrayElement(\@r); my @table_array = split(/AS/, $table_string); $table_array[1] }
 ;


############################################
## Ref: ISO 9075, Chap 7.6 <where clause> ##
############################################
where_clause:
   # <where_clause> is optional
 | WHERE search_condition
 ;

################################################################################
# We ensure that a GROUP BY statement includes all nonaggregates.              #
# This helps to ensure the query is more useful in detecting real errors /     #
# that the query doesn't lend itself to variable result sets                   #
################################################################################
###############################################
## Ref: ISO 9075, Chap 7.7 <group by clause> ##
###############################################
group_by_clause:
   group_by_iff_aggregate
 | group_by_iff_aggregate
 | group_by_iff_aggregate
 | group_by_iff_aggregate
 | group_by_iff_aggregate
 | group_by_iff_aggregate
 | group_by
 ;

## Conditionaly  specify 'GROUP BY' if table expression is a grouped table
group_by_iff_aggregate:
   { scalar(@aggregates) > 0 and scalar(@nonaggregates) > 0 ? " GROUP BY ".join (', ' , @nonaggregates ) : "" }
 ;

group_by:
   { scalar(@nonaggregates) > 0 ? " GROUP BY ".join (', ' , @nonaggregates ) : "" }
 ;

#############################################
## Ref: ISO 9075, Chap 7.8 <having clause> ##
#############################################
having_clause:
   HAVING having_list
 |||||
 ;

having_list:
   having_item
 | having_item
 | (having_list and_or having_item)
 ;

having_item:
   existing_select_item comparison_operator _digit
 ;

and_or:
   AND
 | AND
 | OR
 ;

###################################################
## Ref: ISO 9075, Chap 7.9 <query specification> ##
###################################################
query_spec:
   SELECT lookahead_for_table_alias
   distinct straight_join select_option select_list table_expr
 ;

distinct:  |||| DISTINCT ;

select_option:
 | SQL_SMALL_RESULT
#| SQL_BIG_RESULT     # Need fix for Bug#53534
 |||||||
 ;

straight_join:  ||||||||||| STRAIGHT_JOIN ;

# Tuned to prefer short <select list>'s
select_list:
   select_item
 | select_item, select_item
 | select_item, select_item
 | select_item, select_list
 ;

select_item:
   int_select_item
 | char_select_item
 ;

int_select_item:
   int_value | int_value  | int_value | int_value | int_value
#| int_aggregate     # Need fix for bug#53485
 ;

char_select_item:
   char_value | char_value | char_value | char_value | char_value
#| char_aggregate    # Need fix for bug#53485
 ;

int_value:
   int_value_expr AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 | int_column AS nonaggregate_field
 ;

char_value:
   char_value_expr AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 | char_column AS nonaggregate_field
 ;

int_aggregate:
   COUNT(*) AS aggregate_field
 | COUNT(distinct value_expr) AS aggregate_field
 | SUM(distinct int_value_expr) AS aggregate_field
 | MIN(distinct int_value_expr) AS aggregate_field 
 | MAX(distinct int_value_expr) AS aggregate_field
 ;

char_aggregate:
   MIN(distinct char_value_expr) AS aggregate_field 
 | MAX(distinct char_value_expr) AS aggregate_field 
 ;

nonaggregate_field:
   { my $f = "field".++$fields; push @nonaggregates, $f ; $f }
 ;

aggregate_field:
   { my $f = "field".++$fields; push @aggregates, $f ; $f }
 ;

#################################################
## Ref: ISO 9075, Chap 7.10 <query expression> ##
#################################################
query_expr:
   non_join_query_expr
#| joined_table              # MySql unsupported
 ;

non_join_query_expr:
   non_join_query_term
#| query_expr UNION query_term
#| query_expr EXCEPT query_term
 ;

query_term:
   non_join_query_term
#| joined_table              # MySql unsupported
 ;

non_join_query_term:
   non_join_query_primary
#| query_term INTERSECT query_primary
 ;

query_primary:
   non_join_query_primary
#| joined_table              # MySql unsupported
 ;

non_join_query_primary:
   simple_table
 | simple_table
 | simple_table
 | (non_join_query_expr)
 ;

simple_table:
   query_spec
#| table_value_constr        # MySql unsupported
#| explicit_table            # MySql unsupported
 ;

explicit_table:
   TABLE table
 ;

corresponding_spec:
   CORRESPONDING
 ;

#########################################
## Ref: ISO 9075, Chap 7.11 <subquery> ##
#########################################

int_scalar_subquery:
   int_subquery_1row
 ;

char_scalar_subquery:
   char_subquery_1row
 ;

int_table_subquery:
   int_subquery_1row
 ;

char_table_subquery:
   char_subquery_1row
 ;

int_subquery_1row:
   subqry_enter
   (SELECT lookahead_for_table_alias
    distinct straight_join select_option int_select_item table_expr)
   subqry_leave
 ;

char_subquery_1row:
   subqry_enter
   (SELECT lookahead_for_table_alias
    distinct straight_join select_option char_select_item table_expr)
   subqry_leave
 ;

table_subquery:
   subquery
 ;

subquery:
   subqry_enter
   (query_expr)
   subqry_leave
 ;

subqry_enter:
   { $stack->push(); undef }
   { $stack->set("tables",$tables); $tables = 0; $ext = $ext."s"; undef}
   { my @tmp = @outer_tables; $stack->set("outer_tables",\@tmp); @outer_tables = @table_refs; undef}
   { my @tmp = @table_refs; $stack->set("table_refs",\@tmp); @table_refs = (); undef}
   { my @tmp = @nonaggregates; $stack->set("nonaggregates",\@tmp); @nonaggregates = (); undef}
   { my @tmp = @aggregates; $stack->set("aggregates",\@tmp); @aggregates = (); undef}
 ;

subqry_leave:
   { $tables = $stack->get("tables"); chop $ext; undef }
   { @outer_tables = @{$stack->get("outer_tables")}; undef }
   { @table_refs = @{$stack->get("table_refs")}; undef }
   { @aggregates = @{$stack->get("aggregates")}; undef }
   { @nonaggregates = @{$stack->get("nonaggregates")}; undef}
   { $stack->pop(); undef}
 ;

correlate_iff_subquery_unused:
   { scalar(@outer_tables) > 0 ? $prng->arrayElement(\@table_refs).".pk "."= ".$prng->arrayElement(\@outer_tables).".pk " : "" }
 ;


#################################################
## Ref: ISO 9075, Chap 8.12 <search condition> ##
#################################################
search_condition:
   boolean_factor
 | boolean_factor
 | boolean_factor
 | boolean_factor
 | boolean_factor
 | search_condition and_or boolean_factor
 ;

boolean_factor:
   not boolean_primary is_truth_value
 ;

is_truth_value:
   IS truth_value
   ||||||||||||||||
 ;

truth_value:
   TRUE | FALSE | UNKNOWN
 ;

boolean_primary:
   predicate
 | predicate
 | predicate
 | predicate
 | (search_condition)
 ;

predicate:
   common_int_predicate
 | common_int_predicate
 | common_int_predicate
 | common_int_predicate
 | common_int_predicate
 | common_char_predicate
 | other_predicate
 ;

common_int_predicate:
   int_column comparison_operator _digit
 | int_column comparison_operator _digit
 | int_column comparison_operator _digit
 | int_column comparison_operator int_column
 | int_column not BETWEEN _digit[invariant] AND _digit[invariant]+_digit
 ;

common_char_predicate:
   char_column comparison_operator char_column
 ;

other_predicate:
   int_value_expr comparison_operator int_value_expr
 | char_value_expr comparison_operator char_value_expr
 | value_expr IS not NULL
 | int_value_expr not BETWEEN int_value_expr AND int_value_expr
 | char_value_expr not LIKE {"'%".$prng->string(3)."%'"}
 | EXISTS table_subquery
 | int_value_expr not IN (number_list)
 | int_value_expr not IN int_table_subquery
 | char_value_expr not IN char_table_subquery
 | int_value_expr comparison_operator quantifier int_table_subquery
 | char_value_expr comparison_operator quantifier char_table_subquery
#| UNIQUE table_subquery              # MySql unsupported
 ;

number_list:
   _digit | number_list, _digit
 ;

quantifier:
   ALL | ANY
 ;

################################################################################
# We use the total_order_by rule when using the LIMIT operator to ensure that  #
# we have a consistent result set - server1 and server2 should not differ      #
################################################################################

order_by_clause:
   ORDER BY total_order_by desc limit
 | ORDER BY order_by_list
 |||||
 ;

total_order_by:
   { my @tmp = (@nonaggregates, @aggregates); $prng->shuffleArray(\@tmp); join(',', @tmp) }
 ;

order_by_list:
   order_by_item
 | order_by_item, order_by_list
 ;

order_by_item:
   existing_select_item desc
 ;

desc:
   ASC
 | DESC
 ||||| 
 ; 

################################################################################
# We mix digit and _digit here.  We want to alter the possible values of LIMIT #
# To ensure we hit varying EXPLAIN plans, but the OFFSET can be smaller        #
################################################################################

limit:
   LIMIT limit_size
 | LIMIT limit_size OFFSET _digit
 |||
 ;

field_name:
   int_field_name
 | char_field_name
 | datetime_field_name
 ;

int_field_name:
   pk | col_int_key | col_int_unique | col_int
 ;

int_indexed:
   pk | col_int_key | col_int_unique
 ;

#################################################################
# Charset and collation should be specified when starting mysqld
# or creating the database.
# In order to get a deterministic resultset for 'ORDER BY ... LIMIT'
# ensure that the collation is case sensitive (or binary).
#################################################################
char_field_name:
   col_char_16
 | col_char_16_key
 | col_char_16_unique
 | col_varchar_10
 | col_varchar_10_key
 | col_varchar_10_unique
 | col_varchar_256
 | col_varchar_256_key
 | col_varchar_256_unique
 ; 

char_indexed:
   col_char_16_key
 | col_char_16_key
 | col_varchar_10_key
 | col_varchar_10_unique
 | col_varchar_256_key
 | col_varchar_256_unique
 ;

datetime_field_name:
   col_datetime
 | col_datetime_key
 | col_datetime_unique
 ;

datetime_indexed:
   col_datetime_key
 | col_datetime_unique
 ;

existing_table_item:
   { "table".$prng->int(1,$tables) }
 ;

existing_select_item:
   { my @tmp = (@nonaggregates, @aggregates); $prng->arrayElement(\@tmp) }
 ;

_digit:
    1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
    1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | _tinyint_unsigned
 ;
 
comparison_operator:
   = | = | = | = | =
 | > | < | != | <> | <= | >=
 ;

not:
   NOT |||||||||||| 
;

################################################################################
# We define LIMIT_rows in this fashion as LIMIT values can differ depending on #
# how large the LIMIT is - LIMIT 2 = LIMIT 9 != LIMIT 19                       #
################################################################################

limit_size:
    1 | 2 | 10 | 100 | 1000;

