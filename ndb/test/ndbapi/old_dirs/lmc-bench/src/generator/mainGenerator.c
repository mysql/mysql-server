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

#include <ndb_global.h>

#include <NdbHost.h>
#include <NdbSleep.h>
#include <NdbThread.h>
#include <NdbMain.h>

#include "userInterface.h"
#include "dbGenerator.h"


static int   numProcesses;
static int   numTransactions;
static int   numSeconds;
static int   numWarmSeconds;
static char *testDbName;

static ThreadData data[100];

typedef struct {
   pthread_t threadId;
   int waitSeconds;
   int toExit;
}CheckpointData;

static void usage(char *prog)
{
   char  *progname;

   /*--------------------------------------------*/
   /* Get the name of the program (without path) */
   /*--------------------------------------------*/
   progname = strrchr(prog, '/');

   if (progname == 0)
      progname = prog;
   else
      ++progname;

   fprintf(stderr,
           "Usage: %s [-db <name>] [-proc <num>] [-transactions <num>] [-time <num>]\n"
           "  -db <name>          Specifies the database name\n"
           "                      default = '%s'\n"
           "  -proc <num>         Specifies that <num> is the number of\n"
           "                      concurrent processes. The default is 1.\n"
           "  -transactions <num> Specifies that <num> transactions will be\n"
           "                      performed. The default is to do a specific time interval\n"
           "  -time <num>         Specifies that the test will run for <num> sec.\n"
           "                      The default is 10 sec\n"
           "  -warm <num>         Specifies the warm-up/cooldown period of <num> sec.\n"
           "                      The default is 10 sec\n",
           progname, DEFAULTDB);
   exit(1);
}

static void parse_args(int argc,char **argv)
{
   int i;

   testDbName      = DEFAULTDB;
   numProcesses    = 1;
   numTransactions = 0;
   numSeconds      = 10;
   numWarmSeconds  = 10;

   i = 1;
   while (i < argc){
      if (strcmp("-db",argv[i]) == 0) {
         if (i + 1 >= argc) {
           usage(argv[0]);
           exit(1);
         }
         testDbName = argv[i + 1];
         i += 2;
      }
      else if (strcmp("-proc",argv[i]) == 0) {
         if (i + 1 >= argc) {
            usage(argv[0]);
            exit(1);
         }
         if (sscanf(argv[i+1], "%d", &numProcesses) == -1 ||
             numProcesses <= 0 || numProcesses > 99) {
            fprintf(stderr, "-proc flag requires a positive integer argument [1..99]\n");
            usage(argv[0]);
            exit(1);
         }
         i += 2;
      }
      else if (strcmp("-transactions",argv[i]) == 0) {
         if (i + 1 >= argc) {
            usage(argv[0]);
            exit(1);
         }
         if (sscanf(argv[i+1], "%d", &numTransactions) == -1 ||
             numTransactions < 0) {
            fprintf(stderr, "-transactions flag requires a positive integer argument\n");
            usage(argv[0]);
            exit(1);
         }
         i += 2;
      }
      else if (strcmp("-time",argv[i]) == 0) {
         if (i + 1 >= argc) {
            usage(argv[0]);
            exit(1);
         }
         if (sscanf(argv[i+1], "%d", &numSeconds) == -1 ||
             numSeconds < 0) {
            fprintf(stderr, "-time flag requires a positive integer argument\n");
            usage(argv[0]);
            exit(1);
         }
         i += 2;
      }
      else if (strcmp("-warm",argv[i]) == 0) {
         if (i + 1 >= argc) {
            usage(argv[0]);
            exit(1);
         }
         if (sscanf(argv[i+1], "%d", &numWarmSeconds) == -1 ||
             numWarmSeconds < 0) {
            fprintf(stderr, "-warm flag requires a positive integer argument\n");
            usage(argv[0]);
            exit(1);
         }
         i += 2;
      }
      else
         usage(argv[0]);
   }
}

static void print_transaction(const char            *header,
		              unsigned long          totalCount,
		              TransactionDefinition *trans,
		              unsigned int           printBranch,
		              unsigned int           printRollback)
{
   double f;

   printf("  %s: %d (%.2f%%) Time: %.4f sec TPS = %.0f\n", 
	  header,
          trans->count,
          (double)trans->count / (double)totalCount * 100.0,
          trans->benchTime,
          trans->tps);

   if( printBranch ){
      if( trans->count == 0 )
         f = 0.0;
      else
         f = (double)trans->branchExecuted / (double)trans->count * 100.0;
      printf("      Branches Executed: %d (%.2f%%)\n", trans->branchExecuted, f);
   }

   if( printRollback ){
      if( trans->count == 0 )
         f = 0.0;
      else
         f = (double)trans->rollbackExecuted / (double)trans->count * 100.0;
      printf("      Rollback Executed: %d (%.2f%%)\n", trans->rollbackExecuted, f);
   }
}

void print_stats_sync(const char       *title,
		      unsigned int      length,
		      unsigned int      transactionFlag,
		      GeneratorStatistics *gen,
		      int numProc)
{
   int    i;
   char buf[10];
   char name[100];

   name[0] = 0;
   NdbHost_GetHostName(name);
   
   printf("\n------ %s ------\n",title);
   printf("Length        : %d %s\n",
          length,
          transactionFlag ? "Transactions" : "sec");
   printf("Processor     : %s\n", name);
   printf("Number of Proc: %d\n",numProc);
   printf("\n");

   if( gen->totalTransactions == 0 ) {
      printf("   No Transactions for this test\n");
   }
   else {
      for(i = 0; i < 5; i++) {
         sprintf(buf, "T%d",i+1);
         print_transaction(buf,
                           gen->totalTransactions,
                           &gen->transactions[i],
                           i >= 2,
                           i >= 3 );
      }

      printf("\n");
      printf("  Overall Statistics:\n");
      printf("     Transactions: %d\n", gen->totalTransactions);
      printf("     Inner       : %.0f TPS\n",gen->innerTps);
      printf("     Outer       : %.0f TPS\n",gen->outerTps);
      printf("\n");
   }
}

static void *threadRoutine(void *arg)
{
   UserHandle *uh;
   ThreadData *data = (ThreadData *)arg;
   
   uh = userDbConnect(0, testDbName);
   NdbSleep_MilliSleep(data->threadId);
   dbGenerator(uh,data);
   userDbDisconnect(uh);

   pthread_exit(0);
   return(0);
}

NDB_COMMAND(DbGenerator, "DbGenerator", "DbGenerator", "DbGenerator", 16384)
{
   int i;
   int j;
   GeneratorStatistics  stats;
   GeneratorStatistics *p;
   CheckpointData cd;

   parse_args(argc,argv);

   printf("\nStarting Test with %d process(es) for %d %s\n",
           numProcesses,
           numTransactions ? numTransactions : numSeconds,
           numTransactions ? "Transactions" : "sec");
   printf("   WarmUp/coolDown = %d sec\n", numWarmSeconds);

   /*
   cd.waitSeconds = 300;
   cd.toExit      = 0;
   pthread_create(&cd.threadId, 0, checkpointRoutine, &cd);
   */

   for(i = 0; i < numProcesses; i++) {
      data[i].warmUpSeconds   = numWarmSeconds;
      data[i].testSeconds     = numSeconds;
      data[i].coolDownSeconds = numWarmSeconds;
      data[i].numTransactions = numTransactions;
      data[i].randomSeed      = time(0)+i;
      j = pthread_create(&data[i].threadId, 0, threadRoutine, &data[i]);
      if(j != 0){
	perror("Failed to create thread");
      }
   }

   /*--------------------------------*/
   /* Wait for all processes to exit */
   /*--------------------------------*/
   for(i = 0; i < numProcesses; i++)
      pthread_join(data[i].threadId, 0);

   printf("All threads have finished\n");

   cd.toExit = 1;

   /*-------------------------------------------*/
   /* Clear all structures for total statistics */
   /*-------------------------------------------*/
   stats.totalTransactions = 0;
   stats.outerTps          = 0.0;
   stats.innerTps          = 0.0;

   for(i = 0; i < NUM_TRANSACTION_TYPES; i++ ) {
      stats.transactions[i].benchTime        = 0.0;
      stats.transactions[i].count            = 0;
      stats.transactions[i].tps              = 0.0;
      stats.transactions[i].branchExecuted   = 0;
      stats.transactions[i].rollbackExecuted = 0;
   }

   /*--------------------------------*/
   /* Add the values for all Threads */
   /*--------------------------------*/
   for(i = 0; i < numProcesses; i++) {
      p = &data[i].generator;

      stats.totalTransactions += p->totalTransactions;
      stats.outerTps          += p->outerTps;
      stats.innerTps          += p->innerTps;

      for(j = 0; j < NUM_TRANSACTION_TYPES; j++ ) {
         stats.transactions[j].benchTime        += p->transactions[j].benchTime;
         stats.transactions[j].count            += p->transactions[j].count;
         stats.transactions[j].tps              += p->transactions[j].tps;
         stats.transactions[j].branchExecuted   += p->transactions[j].branchExecuted;
         stats.transactions[j].rollbackExecuted += p->transactions[j].rollbackExecuted;
      }
   }

   print_stats_sync("Test Results", 
		    numTransactions ? numTransactions : numSeconds,
		    numTransactions ? 1 : 0,
		    &stats,
		    numProcesses);

   return(0);
}
