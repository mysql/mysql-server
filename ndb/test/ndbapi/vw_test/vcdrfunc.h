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

/********************************************************/
/* Common functions 					*/
/* unix_ps	checks if a process is running with a	*/
/*		name and pid rc 0=not running 		*/
/*		1=Running				*/
/* logname	create a log filename			*/
/*		Parm					*/
/*		1	lvl1 name			*/
/*		2	lvl2 name			*/
/*		3	lvl3 name			*/
/* m2log	Skriv log rader	som moder		*/
/*		Parm					*/
/*		1	pointer to filehandler		*/
/*		2	Log text max 600 tecken		*/
/* c2log	Skriv log rader	som barn 		*/
/*		Parm					*/
/*		1	pointer to filehandler		*/
/*		2	Log text max 600 tecken		*/
/* n2log	Skriv log rader	utan relation		*/
/*		Parm					*/
/*		1	pointer to filehandler		*/
/*		2	Log text max 600 tecken		*/
/********************************************************/

int n2log(FILE *fi,char *text);
int m2log(FILE *fi,char *text);
int c2log(FILE *fi,char *text);
int checkchangelog(FILE* fp,char *filename);
void logname(char *filename, char *lvl1, char *lvl2, char *lvl3);
void logname_unique_day(char *filename, char *lvl1, char *lvl2, char *lvl3);
int unix_ps(char *proc_name,char *pid);
/*
int unix_ps2(char *proc_name,char *pid);
*/
int unix_ps3(char *proc_name);
int replacetoken(const char* instring,char token,char replace);
int CompAsciiNum(char *, char *);
int CompareIt(char *,char *);
int CompCdrNum(const void *,const void *,void *);
