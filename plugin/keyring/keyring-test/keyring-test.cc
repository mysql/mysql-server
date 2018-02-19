/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#undef HAVE_PSI_INTERFACE
#include "../keyring.cc"
#include <sql_plugin_ref.h>
#include <time.h>
#include <atomic>
#include <iostream>

bool random_keys = false;
bool verbose;
bool generate_random_keys_data = false;
std::atomic<int> number_of_keys_added{0};
int number_of_keys_fetched = 0;
int number_of_keys_removed = 0;
std::atomic<int> number_of_keys_generated{0};
int max_generated_key_length = 0;
int number_of_keys_to_generate = 0;

mysql_mutex_t LOCK_verbose;

void *generate(void *arg) {
  my_thread_init();
  int number_of_keys_to_generate = *((int *)arg);

  for (uint i = 0; number_of_keys_generated < number_of_keys_to_generate;
       i = (i + 1) % number_of_keys_to_generate) {
    char key_id[12];    // Key#1000000\0
    char key_type[16];  // KeyType#1000000\0
    char user[13];      // User#1000000\0
    size_t key_len = rand() % 100;

    int key_nr = random_keys ? rand() % number_of_keys_to_generate : i;
    sprintf(key_id, "Key#%d", key_nr);
    strcpy(key_type, "AES");
    sprintf(user, "User#%d", key_nr);

    bool result = false;

    if ((result = mysql_key_generate(reinterpret_cast<const char *>(key_id),
                                     reinterpret_cast<const char *>(key_type),
                                     reinterpret_cast<const char *>(user),
                                     key_len)) == false)
      ++number_of_keys_generated;

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key generate " << key_id << ' ' << key_type << ' ' << user
                << ' ';
      if (result == false)
        std::cout << "successfull" << std::endl;
      else
        std::cout << "failed" << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
  }
  my_thread_end();
  return NULL;
}

void *store(void *arg) {
  my_thread_init();
  int number_of_keys_to_store = *((int *)arg);

  for (uint i = 0; number_of_keys_added < number_of_keys_to_store;
       i = (i + 1) % number_of_keys_to_store) {
    char key_id[12];    // Key#1000000\0
    char key_type[16];  // KeyType#1000000\0
    char user[13];      // User#1000000\0
    uchar key_stack[] = "KEeeeeeeeEEEEEeeeeEEEEEEEEEEEEY!";
    uchar *key;
    size_t key_len;  //= rand() % 100;
    if (generate_random_keys_data) {
      key_len = rand() % max_generated_key_length;
      key = (uchar *)my_malloc(keyring::key_memory_KEYRING, key_len, MYF(0));
      assert(key != NULL);
      assert(my_rand_buffer(key, key_len) == false);
    } else {
      key = key_stack;
      key_len = strlen(reinterpret_cast<char *>(key)) + 1;
    }

    int key_nr = random_keys ? rand() % number_of_keys_to_store : i;
    sprintf(key_id, "Key#%d", key_nr);
    strcpy(key_type, "AES");
    sprintf(user, "User#%d", key_nr);

    bool result = false;

    if ((result = mysql_key_store(reinterpret_cast<const char *>(key_id),
                                  reinterpret_cast<const char *>(key_type),
                                  reinterpret_cast<const char *>(user), key,
                                  key_len)) == false)
      ++number_of_keys_added;

    if (generate_random_keys_data) my_free(key);

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key store " << key_id << ' ' << key_type << ' ' << user
                << ' ';
      if (result == false)
        std::cout << "successfull" << std::endl;
      else
        std::cout << "failed" << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
  }
  my_thread_end();
  return NULL;
}

void *fetch(void *arg) {
  my_thread_init();
  int number_of_keys_to_fetch = *((int *)arg);

  for (uint i = 0; number_of_keys_fetched < number_of_keys_to_fetch;
       i = (i + 1) % number_of_keys_to_fetch) {
    char key_id[12];  // Key#1000000\0
    char *key_type = NULL;
    char user[13];  // User#1000000\0
    char key[] = "KEeeeeeeeEEEEEeeeeEEEEEEEEEEEEY!";

    int key_nr = random_keys ? rand() % number_of_keys_to_fetch : i;
    sprintf(key_id, "Key#%d", key_nr);
    sprintf(user, "User#%d", key_nr);

    void *key_data = NULL;
    size_t key_len = 0;

    bool result = true;

    if ((result =
             mysql_key_fetch(reinterpret_cast<const char *>(key_id), &key_type,
                             reinterpret_cast<const char *>(user), &key_data,
                             &key_len)) == false &&
        key_data != NULL) {
      ++number_of_keys_fetched;
      if (generate_random_keys_data == false &&
          number_of_keys_to_generate == 0) {
        assert(key_len == strlen(key) + 1);
        assert(strcmp(reinterpret_cast<const char *>(
                          reinterpret_cast<uchar *>(key_data)),
                      key) == 0);
      }
      my_free(key_data);
    }

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key fetch " << key_id << ' ';
      if (key_type != NULL) std::cout << key_type << ' ';
      std::cout << user << ' ';
      if (result == false)
        std::cout << "successfull" << std::endl;
      else
        std::cout << "failed" << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
    if (key_type != NULL) my_free(key_type);
  }
  my_thread_end();
  return NULL;
}

void *remove(void *arg) {
  my_thread_init();
  int number_of_keys_to_remove = *((int *)arg);

  for (uint i = 0; number_of_keys_removed < number_of_keys_to_remove;
       i = (i + 1) % number_of_keys_to_remove) {
    char key_id[12];  // Key#1000000\0
    char user[13];    // User#1000000\0

    int key_nr = random_keys ? rand() % number_of_keys_to_remove : i;
    sprintf(key_id, "Key#%d", key_nr);
    sprintf(user, "User#%d", key_nr);

    bool result = true;

    if ((result = mysql_key_remove(reinterpret_cast<const char *>(key_id),
                                   reinterpret_cast<const char *>(user))) ==
        false)
      ++number_of_keys_removed;

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key remove " << key_id << ' ' << user << ' ';
      if (result == false)
        std::cout << "successfull" << std::endl;
      else
        std::cout << "failed" << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
  }
  my_thread_end();
  return NULL;
}

int main(int argc, char **argv) {
  my_thread_global_init();
  mysql_mutex_init(0, &LOCK_verbose, MY_MUTEX_INIT_FAST);

  keyring::system_charset_info = &my_charset_utf8_general_ci;
  srand(time(NULL));
  unsigned long long threads_store_number = 0;
  unsigned long long threads_remove_number = 0;
  unsigned long long threads_fetch_number = 0;
  unsigned long long threads_generate_key_number = 0;
  int number_of_keys_to_store = 0;
  int number_of_keys_to_fetch = 0;
  int number_of_keys_to_remove = 0;
  my_thread_handle *otid;
  unsigned long long i;
  void *tret;

  if (argc != 12) {
    fprintf(stderr,
            "Usage: keyring_test <path_to_keyring_file> <threads_store_number> "
            "<threads_remove_number> <threads_fetch_number> "
            "<threads_generate_key_number> <number_of_keys_to_store> "
            "<number_of_keys_to_fetch> <number_of_keys_to_generate> "
            "<max_generated_key_length> <random_keys> <verbose>\n");
    return -1;
  }

  my_init();

  keyring_file_data_value =
      (char *)my_malloc(PSI_NOT_INSTRUMENTED, strlen(argv[1]) + 1, MYF(0));
  strcpy(keyring_file_data_value, argv[1]);
  threads_store_number = atoll(argv[2]);
  threads_remove_number = atoll(argv[3]);
  threads_fetch_number = atoll(argv[4]);
  threads_generate_key_number = atoll(argv[5]);
  number_of_keys_to_store = atoi(argv[6]);
  number_of_keys_to_fetch = atoi(argv[7]);
  number_of_keys_to_generate = atoi(argv[8]);
  max_generated_key_length = atoi(argv[9]);
  random_keys = atoi(argv[10]);
  verbose = atoi(argv[11]);
  number_of_keys_to_remove =
      number_of_keys_to_store + number_of_keys_to_generate;

  remove(keyring_file_data_value);  // just to be sure there are no leftovers

  unsigned long long threads_number =
      threads_store_number + threads_fetch_number + threads_remove_number +
      threads_generate_key_number;

  if (!(otid = (my_thread_handle *)my_malloc(
            PSI_NOT_INSTRUMENTED, threads_number * sizeof(*otid), MYF(0))))
    return -2;

  st_plugin_int plugin_info;
  plugin_info.name.str = const_cast<char *>("keyring_plugin");
  plugin_info.name.length = strlen("keyring_plugin");

  if (keyring_init(&plugin_info)) return 1;

  for (i = 0; i < threads_store_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED, &otid[i], NULL, store,
                            (void *)&number_of_keys_to_store))
      return 2;

  for (i = 0; i < threads_fetch_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED,
                            &otid[threads_store_number + i], NULL, fetch,
                            (void *)&number_of_keys_to_fetch))
      return 3;

  for (i = 0; i < threads_remove_number; i++)
    if (mysql_thread_create(
            PSI_NOT_INSTRUMENTED,
            &otid[threads_store_number + threads_fetch_number + i], NULL,
            remove, (void *)&number_of_keys_to_remove))
      return 4;

  for (i = 0; i < threads_generate_key_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED,
                            &otid[threads_store_number + threads_fetch_number +
                                  threads_remove_number + i],
                            NULL, generate,
                            (void *)&number_of_keys_to_generate))
      return 6;

  for (i = 0; i < threads_number; i++) my_thread_join(&otid[i], &tret);

  remove(keyring_file_data_value);
  my_free(keyring_file_data_value);
  my_free(otid);
  mysql_mutex_destroy(&LOCK_verbose);

  keyring_deinit(NULL);
  my_end(0);

  return 0;
}
