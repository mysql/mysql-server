/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/********************************************************************************** 
						test_compiler.cpp 
						Tests the code tree generated
						by the compiler
***********************************************************************************/
#include <stdio.h>
#include <memory.h>
#include "SQL_compiler.hpp"
#include "SQL_code_tree.hpp"

typedef struct stSTMT_REF_tag {
	char* szTestStmt ;
	SQL_compiler* pC ;
	SQL_code_tree* pRefTree ;
	int nFlag ; /* indicate if the struct haa a code tree and compiler and  should be processed */
} stSTMT_REF ;

int compare_trees(SQL_code_tree* pCompilerOutput, SQL_code_tree* pReference) ;

/* Assign statements to szTestStmt and NULL to pTestRef */

static stSTMT_REF stTestRef[] = {
/*  0 */	{"create table foo (pk integer primary key, a integer, b varchar(20), check (a is not null))", NULL, NULL, 0},
/*  1 */	{"insert into foo (pk, a, b) values (1, 10, 'ett')", NULL, NULL, 0},
/*  2 */	{"insert into foo values (2, 20)", NULL, NULL, 0},
/*  3 */	{"delete from foo", NULL, NULL, 1}, 
/*  4 */	{"delete from foo where pk=5", NULL, NULL, 0},
/*  5 */	{"delete from foo where a<10 or b='test'", NULL, NULL, 0},
/*  6 */	{"update foo set a=100, b=null", NULL, NULL, 0},
/*  7 */	{"update foo set a=0 where pk=1", NULL, NULL, 0},
/*  8 */	{"update foo set a=a+pk where b is null", NULL, NULL, 0},
/*  9 */	{"select * from foo", NULL, NULL, 0},
/* 10 */	{"select pk, a, b from foo where pk=1", NULL, NULL, 0},
/* 11 */	{"select * from foo order by a", NULL, NULL, 0},
/* 12 */	{"select * from foo A, foo B where A.pk=B.a and A.a<2*B.a", NULL, NULL, 0}
} ;


int main(int argc, char* argv[]){
	
	int retcode = 0 ;
	int nTests = sizeof(stTestRef)/sizeof(stSTMT_REF) ;
	
	for(int c = 0 ; c < nTests ; c++) {
		if(stTestRef[c].nFlag){
			stTestRef[c].pC = new SQL_compiler() ;
			stTestRef[c].pRefTree = new SQL_code_tree() ;
		}
	}

	/* Create reference code trees */

	/* 
	Statement: 0 "create table foo (pk integer primary key, a integer, b varchar(20), check (a is not null))"
	*/
	

	/* 
	Statement: 1
	*/



	/* 
	Statement: 2
	*/



	/* 
	Statement: 3 "delete from foo"
	*/
	
	stTestRef[3].pRefTree->shift('N') ;
	stTestRef[3].pRefTree->shift('D') ;
	stTestRef[3].pRefTree->shift('B') ;
	stTestRef[3].pRefTree->reduce(0x2050400e, 3) ;
	stTestRef[3].pRefTree->shift('F') ;
    stTestRef[3].pRefTree->shift('O') ;
	stTestRef[3].pRefTree->shift('O') ;
	stTestRef[3].pRefTree->reduce(0x20502003, 3) ;
	stTestRef[3].pRefTree->reduce(0x2050400f, 1) ;
	stTestRef[3].pRefTree->reduce(0x20504007, 2) ;
	stTestRef[3].pRefTree->reduce(0x21407003, 1) ;
	stTestRef[3].pRefTree->shift(0x205021ca) ;
	stTestRef[3].pRefTree->reduce(0x20630001, 1) ;
	stTestRef[3].pRefTree->reduce(0x20815001, 1) ;
	stTestRef[3].pRefTree->shift(0x21407002) ;
	stTestRef[3].pRefTree->reduce(0x21407004, 3) ;
	stTestRef[3].pRefTree->shift(0x21407002) ;
	stTestRef[3].pRefTree->reduce(0x21407005, 1) ;
	stTestRef[3].pRefTree->shift(0x21414001) ;
	stTestRef[3].pRefTree->shift(0x21414002) ;
	stTestRef[3].pRefTree->reduce(0x21407001, 4) ;
	stTestRef[3].pRefTree->reduce(0x51506004, 1) ;
	stTestRef[3].pRefTree->reduce(0x51506003, 1) ;
	
	/* 
	Statement: 4
	*/



	/* 
	Statement: 5
	*/




	/* 
	Statement: 6
	*/




	/* 
	Statement: 7
	*/



	/* 
	Statement: 8
	*/



	/* 
	Statement: 9
	*/



	/* 
	Statement: 10
	*/



	/* 
	Statement: 11
	*/



	/* 
	Statement: 12
	*/



	for(int i = 0 ; i < nTests ; i++){
		/* Check to see if the statement has an associated code tree and compiler */
		if(stTestRef[i].nFlag){
			stTestRef[i].pC->prepare( stTestRef[i].szTestStmt, strlen(stTestRef[i].szTestStmt)) ;
			if( 0 != compare_trees(&stTestRef[i].pC->m_code_tree, stTestRef[i].pRefTree) ){
				printf("\nCompiler generated tree for statement #%d: \"%s\"\ndeviates from its reference\n", i, stTestRef[i].szTestStmt) ;
				retcode = -1 ;
				break ;
			}else{
				printf("\nTrees for statement #%d: \"%s\" match nicely -- OK\n", i, stTestRef[i].szTestStmt) ;
				retcode = 0 ;
			}
		}
	}
	
	for(int d = 0 ; d < nTests ; d++) {
		if(stTestRef[d].nFlag){
			delete stTestRef[d].pC ;
			delete stTestRef[d].pRefTree ;
		}
	}
	
	return retcode ;
	
}
	



int compare_trees(SQL_code_tree* pCompilerOutput, SQL_code_tree* pReference){

	int nTop = -1 ;
		
	if(pCompilerOutput->top()== pReference->top()){

		nTop = pReference->top() ;	

	}else{
		printf("\npCompilerOutput->top() = %d;\tpReference->top() = %d\n", pCompilerOutput->top(), pReference->top()) ;
		return -1 ;
	}
		
	pCompilerOutput->beginPostfix() ;
	pReference->beginPostfix() ;

	for(int r = 0 ; r < nTop ; r++){	
		if(pCompilerOutput->symbol() != pReference->symbol()){
			
			printf("Deviation found in position %d\n", r) ;
			printf("pCompilerOutput->symbol() = 0x%X;\tpReference->symbol() = 0x%X\n", pCompilerOutput->symbol(), pReference->symbol()) ;
			return -1 ;

		}else{

			pCompilerOutput->nextPostfix() ;
			pReference->nextPostfix() ;

		}
	}

	return 0 ;

}

