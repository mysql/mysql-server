/*
 Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#include <my_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memcached/extension_loggers.h>

#define HARNESS 1
#include "all_tests.h"

/***  These tests require a running cluster. 
      Some of them require the ndbmemcache.demo_table to exist.  
      If a particular test-id is supplied, run one test verbosely;
      otherwise run all tests and indicate pass or fail.
***/      

char *connect_string;   /* exported to tests */

EXTENSION_LOGGER_DESCRIPTOR *logger;
                       
Ndb_cluster_connection * connect(const char *);

int list_tests(void);
int usage(char *);

int main(int argc, char *argv[]) {
  connect_string = NULL;
  int test_number = -1;
  char * test_name = 0;
  int optc;
  int req_level;
  int npass = 0, nfail = 0;

  Ndb_cluster_connection * conn = NULL;
  QueryPlan *plan = NULL;
  Ndb *db = NULL;
  
  /* Options */
  while((optc = getopt(argc, argv, "hlc:t:")) != -1) {
    switch(optc) {
      case 'c':
        connect_string = optarg;
        break;
      case 't':
        test_number = atoi(optarg);
        if(test_number > 0) test_number -= 1;
        else test_name = optarg;
        break;
      case 'l':
        return list_tests();
        break;
      case 'h':
      default:
        return usage(argv[0]);
        break;
    } 
  }
    
  /* If a test name was given, find it by number */
  if(test_name) {
    for(int i = 0; all_tests[i].name; i++) {
      if(! strcmp(test_name, all_tests[i].name)) {
        test_number = i;
        break;
      }
    }
    if(test_number == -1) {
      printf("Test \"%s\" not found.\n", test_name);
      exit(1);
    }  
  }
  
  /* Determine requirements level for this test */
  if(test_number >= 0)
    req_level = all_tests[test_number].requires;
  else
    req_level = REQ_DEMO_TABLE;  // highest level

  ndb_init();
  DEBUG_INIT(NULL, 0);
  
  if(req_level >= REQ_NDB_CONNECTION) { 
    printf("Connecting to cluster (%s)\n", connect_string);
    conn = connect(connect_string);
    db = new Ndb(conn);
    db->init(4);
  }
  
  if(req_level >= REQ_DEMO_TABLE) {    
    TableSpec spec("ndbmemcache.demo_table", "mkey", "string_value");
    spec.cas_column = "cas_value";
    spec.math_column = "math_value";
    plan = new QueryPlan(db, &spec);
  }

  if(test_number >= 0) {   /* Run a particular test */
    printf("%s\n", all_tests[test_number].name);
    int r = all_tests[test_number].function(plan, db, 1);  //verbose
    if(r) {
      printf(" [FAIL] at line %d\n", r); nfail++;
    } else {
      printf(" [PASS]\n"); npass++; 
    }
  }
  else {                  /* Run all tests */
    for(int i = 0; all_tests[i].name; i++) {
      if(all_tests[i].enabled) {
        printf("%-30s", all_tests[i].name);
        int r = all_tests[i].function(plan, db, 0);   // quiet
        printf(" %s\n", r ? "[FAIL]" : "[PASS]");
        if(r) nfail++; 
        else npass++;
      }
    }
    printf("\nTotals:  %d pass        ...    %d fail\n", npass, nfail);
  }
  
  exit((nfail > 0));
}


int list_tests() {
  printf("\n");
  printf("No. %-30s %-20s %-10s\n", "Name", "Requires","Enabled");
  printf("----------------------------------------------------------------\n");
  for(int i = 0; all_tests[i].name; i++)
    printf("%d   %-30s %-20s %-10s\n", i+1, all_tests[i].name,
           requirements[all_tests[i].requires],
           all_tests[i].enabled ? "Yes" : "No");
  printf("\n");
  return 0;
}


int usage(char *prog) {
  printf("\n");
  printf("usage %s [options]\n", prog);
  printf("options: \n");
  printf("  -c connectstring  : specify NDB connect-string\n");
  printf("  -t test-id        : run a particular test by number or name\n");
  printf("  -l                : list tests\n");
  printf("  -h                : help\n");
  printf("\n");
  
  return 0;
}


Ndb_cluster_connection * connect(const char *connectstring) {
  int conn_retries = 0;
  Ndb_cluster_connection *c = new Ndb_cluster_connection(connectstring);
  
  /* Set name that appears in the cluster log file */
  c->set_name("unit_test");
  
  while(1) {
    conn_retries++;
    int r = c->connect(2,1,0);
    if(r == 0)         // success 
      break;
    else if(r == -1)   // unrecoverable error
      return NULL;
    else if (r == 1) { // recoverable error
      if(conn_retries == 5)
        return NULL;
      else 
        sleep(1);
    }
  }
  
  int ready_nodes = c->wait_until_ready(5, 5);
  if(ready_nodes < 0) {
    printf("Timeout waiting for cluster \"%s\" to become ready (%d).\n", 
            connectstring, ready_nodes);
    return NULL;
  }
  
  printf("Connected to \"%s\" as node id %d.\n", connectstring, c->node_id());
  if(ready_nodes > 0) printf("Only %d storage nodes are ready.\n", ready_nodes);
  
  return c;
}


void delete_row(QueryPlan *plan, Ndb *db, const char * key, int verbose) {
  char ndbkeybuffer[300];
    
  Operation op(plan, OP_DELETE, ndbkeybuffer);  

  op.clearKeyNullBits();
  op.setKeyPart(COL_STORE_KEY, key, strlen(key));
  
  NdbTransaction * tx = op.startTransaction(db);
  op.deleteTuple(tx);
  tx->execute(NdbTransaction::Commit);  

  detail(verbose, "delete \"%s\": %d \n", key, tx->RESULT);
  tx->close();
}
