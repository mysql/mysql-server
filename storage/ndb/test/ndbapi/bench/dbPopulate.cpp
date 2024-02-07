/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/***************************************************************
 * I N C L U D E D   F I L E S                                  *
 ***************************************************************/

#include <ndb_global.h>

#include "userInterface.h"

#include <random.h>
#include <NdbOut.hpp>
#include "dbPopulate.h"
#include "util/require.h"

/***************************************************************
 * L O C A L   C O N S T A N T S                                *
 ***************************************************************/

/***************************************************************
 * L O C A L   D A T A   S T R U C T U R E S                    *
 ***************************************************************/

/***************************************************************
 * L O C A L   F U N C T I O N S                                *
 ***************************************************************/

static void getRandomSubscriberData(int subscriberNo, SubscriberNumber number,
                                    SubscriberName name);

static void populate(const char *title, int count,
                     void (*func)(UserHandle *, int), UserHandle *uh);

static void populateServers(UserHandle *uh, int count);
static void populateSubscribers(UserHandle *uh, int count);
static void populateGroups(UserHandle *uh, int count);

/***************************************************************
 * L O C A L   D A T A                                          *
 ***************************************************************/

static SequenceValues permissionsDefinition[] = {{90, 1}, {10, 0}, {0, 0}};

/***************************************************************
 * P U B L I C   D A T A                                        *
 ***************************************************************/

/***************************************************************
****************************************************************
* L O C A L   F U N C T I O N S   C O D E   S E C T I O N      *
****************************************************************
***************************************************************/

static void getRandomSubscriberData(int subscriberNo, SubscriberNumber number,
                                    SubscriberName name) {
  char sbuf[SUBSCRIBER_NUMBER_LENGTH + 1];
  sprintf(sbuf, "%.*d", SUBSCRIBER_NUMBER_LENGTH, subscriberNo);
  memcpy(number, sbuf, SUBSCRIBER_NUMBER_LENGTH);

  memset(name, myRandom48(26) + 'A', SUBSCRIBER_NAME_LENGTH);
}

static void populate(const char *title, int count,
                     void (*func)(UserHandle *, int), UserHandle *uh) {
  ndbout_c("Populating %d '%s' ... ", count, title);
  /* fflush(stdout); */
  func(uh, count);
  ndbout_c("done");
}

/*
 * nextDecimal takes a number as a string of decimal digits and increment the
 * number with one. All 9's will become all 0's and returning true to indicate
 * that number wrapped to zero, else false is returned.
 */
static bool nextDecimal(char decimal[], size_t len) {
  if (len == 0) return true;
  require(len <= INT_MAX);
  for (int i = len - 1; i >= 0; i--) {
    require('0' <= decimal[i] && decimal[i] <= '9');
    if (decimal[i] < '9') {
      decimal[i]++;
      return false;
    }
    decimal[i] = '0';
  }
  return true;
}

static void populateServers(UserHandle *uh, int count) {
  int commitCount = 0;

  for (int i = 0; i < count; i++) {
    ServerName serverName;
    char tmp[80];
    int len = sprintf(tmp, "-Server %d-", i);
    require(len >= 0);
    for (int j = 0; j < SERVER_NAME_LENGTH; j++) {
      serverName[j] = tmp[j % len];
    }

    SubscriberSuffix suffix;
    memset(suffix, '0', SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
    for (;;) {
      userDbInsertServer(uh, i, suffix, serverName);
      commitCount++;
      if ((commitCount % OP_PER_TRANS) == 0) userDbCommit(uh);

      bool wrapped = nextDecimal(suffix, SUBSCRIBER_NUMBER_SUFFIX_LENGTH);
      if (wrapped) break;
    }
  }
  if ((commitCount % OP_PER_TRANS) != 0) userDbCommit(uh);
}

static void populateSubscribers(UserHandle *uh, int count) {
  SubscriberNumber number;
  SubscriberName name;
  int i, j, k;
  int res;

  SequenceValues values[NO_OF_GROUPS + 1];
  RandomSequence seq;

  for (i = 0; i < NO_OF_GROUPS; i++) {
    values[i].length = 1;
    values[i].value = i;
  }

  values[i].length = 0;
  values[i].value = 0;

  if (initSequence(&seq, values) != 0) {
    ndbout_c("could not set the sequence of random groups");
    exit(0);
  }

#define RETRIES 25

  for (i = 0; i < count; i += OP_PER_TRANS) {
    for (j = 0; j < RETRIES; j++) {
      for (k = 0; k < OP_PER_TRANS && i + k < count; k++) {
        getRandomSubscriberData(i + k, number, name);
        userDbInsertSubscriber(uh, number, getNextRandom(&seq), name);
      }
      res = userDbCommit(uh);
      if (res == 0) break;
      if (res != 1) {
        ndbout_c("Terminating");
        exit(0);
      }
    }
    if (j == RETRIES) {
      ndbout_c("Terminating");
      exit(0);
    }
  }
}

static void populateGroups(UserHandle *uh, int count) {
  int i;
  int j;
  int len;
  RandomSequence seq;
  Permission allow[NO_OF_GROUPS];
  ServerBit serverBit;
  GroupName groupName;
  char tmp[80];
  int commitCount = 0;

  if (initSequence(&seq, permissionsDefinition) != 0) {
    ndbout_c("could not set the sequence of random permissions");
    exit(0);
  }

  for (i = 0; i < NO_OF_GROUPS; i++) allow[i] = 0;

  for (i = 0; i < NO_OF_SERVERS; i++) {
    serverBit = 1 << i;

    for (j = 0; j < NO_OF_GROUPS; j++) {
      if (getNextRandom(&seq)) allow[j] |= serverBit;
    }
  }

  for (i = 0; i < NO_OF_GROUPS; i++) {
    sprintf(tmp, "-Group %d-", i);

    len = (int)strlen(tmp);

    for (j = 0; j < GROUP_NAME_LENGTH; j++) {
      groupName[j] = tmp[j % len];
    }
    /* groupName[j] = 0;	not null-terminated */

    userDbInsertGroup(uh, i, groupName, allow[i], allow[i], allow[i]);
    commitCount++;
    if ((commitCount % OP_PER_TRANS) == 0) userDbCommit(uh);
  }
  if ((commitCount % OP_PER_TRANS) != 0) userDbCommit(uh);
}

/***************************************************************
****************************************************************
* P U B L I C   F U N C T I O N S   C O D E   S E C T I O N    *
****************************************************************
***************************************************************/

extern int subscriberCount;

void dbPopulate(UserHandle *uh) {
  populate("servers", NO_OF_SERVERS, populateServers, uh);
  populate("subscribers", subscriberCount, populateSubscribers, uh);
  populate("groups", NO_OF_GROUPS, populateGroups, uh);
}
