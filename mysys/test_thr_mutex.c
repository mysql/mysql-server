/* Copyright (C) 2008 Sun Microsystems, Inc

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

/* Testing of deadlock detector */

#include <my_global.h>
#include <mysys_priv.h>


int main(int argc __attribute__((unused)), char** argv)
{
  pthread_mutex_t LOCK_A, LOCK_B, LOCK_C, LOCK_D, LOCK_E, LOCK_F, LOCK_G;
  pthread_mutex_t LOCK_H, LOCK_I;
  MY_INIT(argv[0]);
  DBUG_ENTER("main");

  DBUG_PUSH("d:t:O,/tmp/trace");
  printf("This program is testing the mutex deadlock detection.\n"
         "It should print out different failures of wrong mutex usage"
         "on stderr\n\n");

  safe_mutex_deadlock_detector= 1;
  pthread_mutex_init(&LOCK_A, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_B, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_C, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_D, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_E, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_F, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_G, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_H, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_I, MY_MUTEX_INIT_FAST);

  printf("Testing A->B and B->A\n");
  fflush(stdout);
  pthread_mutex_lock(&LOCK_A);
  pthread_mutex_lock(&LOCK_B);
  pthread_mutex_unlock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_B);

  /* Test different (wrong) lock order */
  pthread_mutex_lock(&LOCK_B);
  pthread_mutex_lock(&LOCK_A);                  /* Should give warning */

  pthread_mutex_unlock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_B);

  /* Check that we don't get another warning for same lock */
  printf("Testing A->B and B->A again (should not give a warning)\n");
  pthread_mutex_lock(&LOCK_B);
  pthread_mutex_lock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_B);

  /*
    Test of ring with many mutex
    We also unlock mutex in different orders to get the unlock code properly
    tested.
  */
  printf("Testing A->C and C->D and D->A\n");
  pthread_mutex_lock(&LOCK_A);
  pthread_mutex_lock(&LOCK_C);
  pthread_mutex_unlock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_C);
  pthread_mutex_lock(&LOCK_C);
  pthread_mutex_lock(&LOCK_D);
  pthread_mutex_unlock(&LOCK_D);
  pthread_mutex_unlock(&LOCK_C);

  pthread_mutex_lock(&LOCK_D);
  pthread_mutex_lock(&LOCK_A);                  /* Should give warning */

  pthread_mutex_unlock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_D);

  printf("Testing E -> F ; H -> I ; F -> H ; H -> I -> E\n");
  fflush(stdout);

  pthread_mutex_lock(&LOCK_E);
  pthread_mutex_lock(&LOCK_F);
  pthread_mutex_unlock(&LOCK_E);
  pthread_mutex_unlock(&LOCK_F);
  pthread_mutex_lock(&LOCK_H);
  pthread_mutex_lock(&LOCK_I);
  pthread_mutex_unlock(&LOCK_I);
  pthread_mutex_unlock(&LOCK_H);
  pthread_mutex_lock(&LOCK_F);
  pthread_mutex_lock(&LOCK_H);
  pthread_mutex_unlock(&LOCK_H);
  pthread_mutex_unlock(&LOCK_F);

  pthread_mutex_lock(&LOCK_H);
  pthread_mutex_lock(&LOCK_I);
  pthread_mutex_lock(&LOCK_E);                  /* Should give warning */

  pthread_mutex_unlock(&LOCK_E);
  pthread_mutex_unlock(&LOCK_I);
  pthread_mutex_unlock(&LOCK_H);

  printf("\nFollowing shouldn't give any warnings\n");
  printf("Testing A->B and B->A without deadlock detection\n");
  fflush(stdout);

  /* Reinitialize mutex to get rid of old wrong usage markers */
  pthread_mutex_destroy(&LOCK_A);
  pthread_mutex_destroy(&LOCK_B);
  pthread_mutex_init(&LOCK_A, MY_MUTEX_INIT_FAST);
  pthread_mutex_init(&LOCK_B, MY_MUTEX_INIT_FAST);

  /* Start testing */
  my_pthread_mutex_lock(&LOCK_A, MYF(MYF_NO_DEADLOCK_DETECTION));
  pthread_mutex_lock(&LOCK_B);
  pthread_mutex_unlock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_B);

  pthread_mutex_lock(&LOCK_A);
  my_pthread_mutex_lock(&LOCK_B, MYF(MYF_NO_DEADLOCK_DETECTION));
  pthread_mutex_unlock(&LOCK_A);
  pthread_mutex_unlock(&LOCK_B);

  printf("Testing A -> C ; B -> C ; A->B\n");
  fflush(stdout);
  pthread_mutex_lock(&LOCK_A);
  pthread_mutex_lock(&LOCK_C);
  pthread_mutex_unlock(&LOCK_C);
  pthread_mutex_unlock(&LOCK_A);

  pthread_mutex_lock(&LOCK_B);
  pthread_mutex_lock(&LOCK_C);
  pthread_mutex_unlock(&LOCK_C);
  pthread_mutex_unlock(&LOCK_B);

  pthread_mutex_lock(&LOCK_A);
  pthread_mutex_lock(&LOCK_B);
  pthread_mutex_unlock(&LOCK_B);
  pthread_mutex_unlock(&LOCK_A);

  /* Cleanup */
  pthread_mutex_destroy(&LOCK_A);
  pthread_mutex_destroy(&LOCK_B);
  pthread_mutex_destroy(&LOCK_C);
  pthread_mutex_destroy(&LOCK_D);
  pthread_mutex_destroy(&LOCK_E);
  pthread_mutex_destroy(&LOCK_F);
  pthread_mutex_destroy(&LOCK_G);
  pthread_mutex_destroy(&LOCK_H);
  pthread_mutex_destroy(&LOCK_I);

  my_end(MY_DONT_FREE_DBUG);
  exit(0);
}
