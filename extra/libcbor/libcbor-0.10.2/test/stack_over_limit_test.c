#include "assertions.h"
#include "cbor.h"

static size_t generate_overflow_data(unsigned char **overflow_data) {
  int i;
  *overflow_data = (unsigned char *)malloc(CBOR_MAX_STACK_SIZE + 3);
  for (i = 0; i < CBOR_MAX_STACK_SIZE + 1; i++) {
    (*overflow_data)[i] = 0xC2;  // tag of positive bignum
  }
  (*overflow_data)[CBOR_MAX_STACK_SIZE + 1] = 0x41;  // bytestring of length 1
  (*overflow_data)[CBOR_MAX_STACK_SIZE + 2] = 0x01;  // a bignum of value 1
  return CBOR_MAX_STACK_SIZE + 3;
}

static void test_stack_over_limit(void **_CBOR_UNUSED(_state)) {
  unsigned char *overflow_data;
  size_t overflow_data_len;
  struct cbor_load_result res;
  overflow_data_len = generate_overflow_data(&overflow_data);
  assert_null(cbor_load(overflow_data, overflow_data_len, &res));
  free(overflow_data);
  assert_size_equal(res.error.code, CBOR_ERR_MEMERROR);
}

int main(void) {
  const struct CMUnitTest tests[] = {cmocka_unit_test(test_stack_over_limit)};
  return cmocka_run_group_tests(tests, NULL, NULL);
}
