/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code */

const char **stopwordlist=ft_precompiled_stopwords;

#define MAX_REC_LENGTH 128
#define MAX_BLOB_LENGTH 60000
char record[MAX_REC_LENGTH], read_record[MAX_REC_LENGTH+MAX_BLOB_LENGTH];
char blob_record[MAX_BLOB_LENGTH+20*20];

char *filename= (char*) "EVAL";

int silent=0, error=0;

uint key_length=MAX_BLOB_LENGTH,docid_length=32;
char *d_file, *q_file;
FILE *df,*qf;

MI_COLUMNDEF recinfo[3];
MI_KEYDEF keyinfo[2];
MI_KEYSEG keyseg[10];

void get_options(int argc,char *argv[]);
int create_record(char *, FILE *);

#define SWL_INIT 500
#define SWL_PLUS 50

#define MAX_LINE_LENGTH 128
char line[MAX_LINE_LENGTH];

