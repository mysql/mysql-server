/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
   @mainpage NDB ODBC

   The ODBC Driver Frontend has:
   -# HandleBase  : Various ODBC handles
   -# AttrArea    : Attributes of handles
   -# ConnArea    : Communication area on connection level between driver parts
   -# StmtArea    : Communication area on statement level between driver parts

   and controls the following steps:
   -# SQL_compiler           : Compiles SQL into SQL_code_tree:s
      -# Parser              : Bison grammar
      -# Analyzer            : Syntactic and semantic checks (binds names)
      -# PlanGen             : Generate initial (execution) plan (PlanTree)
   -# CodeGen                : Generates CodeTree:s out of PlanTree:s
      -# Optimizer           : Optimizes PlanTree:s
      -# Output              : Outputs executable CodeTree:s
   -# Executor               : Executes CodeTree:s
      -# CodeTree::allocRun  : Allocates runtime data structures (RunTree:s)
      -# Dataflow machine    : Executes and evaluates statement and expressions

   The Dataflow machine works in four different ways:
   -# Non-query statements
      -# CodeStmt::execute   : Executes (non-query) statement 
   -# Query statements
      -# CodeQuery::execute  : Execute Query statement
      -# CodeQuery::fetch    : Fetch row from CodeQuery node
   -# Boolean expressions
      -# CodePred::evaluate  : Evaluates boolean expression
   -# Arithmetical expressions
      -# CodeExpr::evaluate  : Evaluates arithmetic expression

   The following components are used throughout the NDB ODBC:
   -# Context (Ctx)                   : Info regarding execution/evaluation
   -# Diagnostic area (DiagArea)      : Errors and warnings (for ODBC user)
   -# DescArea : Description of ODBC user input/output bind varibles/columns
   -# Dictionary (DictBase)           : Lookup info stored in NDB Dictionary
                                        and info regarding temporary 
			                materialized results
   -# ResultArea                      : Execution (temporary) results


   @section secCompiler          SQL_compiler : SQL to SQL_code_tree

   The SQL_compiler takes an <em>SQL statement</em> and translates 
   it into an SQL_code_tree.  The compiler uses an SQL_code_tree 
   internally during the compilation and the result of the compilation
   is a simlified SQL_code_tree.

   The compiler works in the following steps:
   -# Parse SQL statments and create SQL_code_tree representing the 
      statement.
   -# Apply Syntax Rules to the SQL_code_tree.  Syntax rules are 
      rules which are <em>not</em> expressed in the SQL grammar,
      but are expressed in natural language in the SQL specification.
   -# Apply Access Rules to the SQL_code_tree 
      (this is not implemented, since NDB Cluster has no access control)
   -# Apply General Rules to the SQL_code_tree
   -# Apply Conformance Rules to the SQL_code_tree

   The resulting simplified SQL_code_tree is represented by a
   tree of C++ objects.


   @section secCodegen           Codegen : SQL_code_tree to CodeTree

   CodeGen takes simplified SQL_code_tree:s and transforms them into
   CodeTree:s.  


   @section secOptimizer         Optimizer : CodeTree to CodeTree

   The implementation of the ODBC optimizer will uses the
   PlanTree:s to represent statements and transforms them
   into executable format (still PlanTree format).

   @note In the future, more optimizations can be implemented.


   @section secExecutor          Executor : Execute CodeTree

   The Executor uses the following data structures:
   -# CodeTree  : A read-only quary evaluation plan
   -# RunTree   : Runtime data structures containing ResultSet:s 

   The execution mechanism is actually implemented as a
   part of the CodeTree.
*/
