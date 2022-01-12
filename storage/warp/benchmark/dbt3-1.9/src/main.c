#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "db.h"

struct sql_statement_t sql_statement;

int main(int argc, char *argv[])
{
	FILE *query_input;
	FILE *query_output;
	char query_input_file_name[1024];
	char query_output_file_name[1024];
	int rc;
	int perf_run_number;
	int stream_number;
	int run_type;

	if (argc < 4)
	{
		printf("usage: \n%s <query_input_file> <query_output_file> <S>\n", argv[0]);
		printf("usage: \n%s <query_input_file> <query_output_file> <E>\n", argv[0]);
		printf("%s <query_input_file> <query_output_file> <P> <perf_run_number> \n", argv[0]);
		printf("%s <query_input_file> <query_output_file> <T> <perf_run_number> <throughput_query_stream_number>\n", argv[0]);
		exit(1);
	}
	
	strcpy(query_input_file_name, argv[1]);
	strcpy(query_output_file_name, argv[2]);
	if (strcmp(argv[3], "P")==0 || strcmp(argv[3],"p")==0)
	{
		run_type = POWER;
		perf_run_number = atoi(argv[4]);
	}
	else if (strcmp(argv[3], "S")==0 || strcmp(argv[3], "s") == 0)
		run_type = SINGLE;
	else if (strcmp(argv[3], "E")==0 || strcmp(argv[3], "e") == 0)
		run_type = EXPLAIN;
	else if (strcmp(argv[3], "T")==0 || strcmp(argv[3], "t") == 0)
	{
		run_type = THROUGHPUT;
		perf_run_number = atoi(argv[4]);
		stream_number = atoi(argv[5]);
	}
	else
	{
		printf("run type: P -- power test  T -- throughput test  S -- single query E -- explain \n");
		exit(1);
	}
	

	if ( (query_input=fopen(query_input_file_name, "r")) == NULL)
	{
		printf("can not open file %s\n", query_input_file_name);
		exit(-1);
	}

	if ( (query_output=fopen(query_output_file_name, "w")) == NULL)
	{
		printf("can not open file %s\n", query_output_file_name);
		exit(-1);
	}

	if (run_type == POWER || run_type == THROUGHPUT)
		fprintf(query_output, "%s\n", SQL_ISOLATION);

	while ( (rc=get_statement(query_input)) != END_OF_FILE)
	{
		/* if this is the first statement in this block */
		if (rc == BEGIN_OF_BLOCK)
		{
			if (run_type == POWER)
				fprintf(query_output, SQL_TIME_P_INSERT, SQL_EXEC, perf_run_number, sql_statement.query_id);
			else if (run_type == THROUGHPUT)
				fprintf(query_output, SQL_TIME_T_INSERT, SQL_EXEC, perf_run_number, stream_number, sql_statement.query_id);
			fprintf(query_output, "%s\n", SQL_COMMIT);
                }
		if (rc == END_OF_STMT)
		{
			if ( run_type == EXPLAIN )
			{
				/* do not get execution plan for Q15 */
				if (sql_statement.query_id != 15)
				{
					fprintf(query_output, "%s %s", SQL_EXEC, sql_statement.statement);
#ifdef SAPDB
					fprintf(query_output, "%s %s", SQL_EXEC, SQL_EXE_PLAN);
#endif /* SAPDB */
				}
			}
			else
			{
					fprintf(query_output, "%s %s", SQL_EXEC, sql_statement.statement);
			}
		}
		if (rc == END_OF_BLOCK)
		{
			if (run_type == POWER)
				fprintf(query_output, SQL_TIME_P_UPDATE, SQL_EXEC, perf_run_number, sql_statement.query_id);
			else if (run_type == THROUGHPUT)
				fprintf(query_output, SQL_TIME_T_UPDATE, SQL_EXEC, perf_run_number, stream_number, sql_statement.query_id);
			fprintf(query_output, "%s\n", SQL_COMMIT);
		}
	}
	
	fclose(query_input);
	fclose(query_output);
	return 1;
}
