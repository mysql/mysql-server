#ifndef CRYPT_HASHGEN_IMPL_H
#define CRYPT_HASHGEN_IMPL_H
#define	ROUNDS_DEFAULT	5000
#define	ROUNDS_MIN	1000
#define	ROUNDS_MAX	999999999
#define	MIXCHARS	32
#define CRYPT_SALT_LENGTH  20
#define CRYPT_MAGIC_LENGTH  3
#define CRYPT_PARAM_LENGTH 13
#define CRYPT_MAX_PASSWORD_SIZE (CRYPT_SALT_LENGTH + \
                                 43 + \
                                 CRYPT_MAGIC_LENGTH + \
                                 CRYPT_PARAM_LENGTH)

#include <stddef.h>

int extract_user_salt(char **salt_begin,
                      char **salt_end);
#ifdef __cplusplus
extern "C" {
#endif
char *
crypt_genhash_impl(char *ctbuffer,
                   size_t ctbufflen,
                   const char *plaintext,
                   int plaintext_len,
                   const char *switchsalt,
                   const char **params);
void generate_user_salt(char *buffer, int buffer_len);
void xor_string(char *to, int to_len, char *pattern, int pattern_len);

#ifdef __cplusplus
} // extern "C"
#endif 
#endif
