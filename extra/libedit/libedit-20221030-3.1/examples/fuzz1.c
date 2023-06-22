/*
 * build:
 *   CC=clang CXX=clang++ CFLAGS="-fsanitize=address,fuzzer-no-link -g" \
 *   	CXXFLAGS="-fsanitize=address,fuzzer-no-link -g" ./configure && make
 * run:
 *   LD_LIBRARY_PATH=../src/.libs/ .libs/fuzz1 -max_len=32 \
 *	-use_value_profile=1 -only_ascii=1
 */
#include <readline/readline.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int init = 0;

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
  if (!Size)
    return 0;

  if (!init) {
    setlocale(LC_CTYPE, "");
    stifle_history(7);
    init = 1;
  }

  clear_history();

  size_t lasti = 0;

  for (size_t i = 0;; ++i) {
    if (i == Size || Data[i] == '\n') {
      if (i - lasti) {
        char *s = (char *)malloc(i - lasti + 1);
        memcpy(s, &Data[lasti], i - lasti);
        s[i - lasti] = '\0';

        char *expansion;
        int result;

#ifdef DEBUG
        fprintf(stderr, "Calling history_expand: >%s<\n", s);
#endif
        result = history_expand(s, &expansion);

        if (result < 0 || result == 2) {
          /* Errors ignored */
        } else {
          add_history(expansion);
        }
        free(expansion);
        free(s);
      }
      lasti = i + 1;
    }

    if (i == Size)
      break;
  }

  return 0;
}
