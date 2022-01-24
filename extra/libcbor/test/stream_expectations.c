#include "stream_expectations.h"

/* Ordered from 0 to queue_size - 1 */
struct test_assertion assertions_queue[MAX_QUEUE_ITEMS];
size_t queue_size = 0;
size_t current_expectation = 0;
decoder_t* decoder;

void set_decoder(decoder_t* dec) { decoder = dec; }

/* Callbacks */
struct test_assertion current() {
  return assertions_queue[current_expectation];
}

/* Assertions builders and matcher callbacks */

void assert_uint8_eq(uint8_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      UINT8_EQ, (union test_expectation_data){.int8 = actual}};
}

void uint8_callback(void* context, uint8_t actual) {
  assert_true(current().expectation == UINT8_EQ);
  assert_true(current().data.int8 == actual);
  current_expectation++;
}

void assert_uint16_eq(uint16_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      UINT16_EQ, (union test_expectation_data){.int16 = actual}};
}

void uint16_callback(void* context, uint16_t actual) {
  assert_true(current().expectation == UINT16_EQ);
  assert_true(current().data.int16 == actual);
  current_expectation++;
}

void assert_uint32_eq(uint32_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      UINT32_EQ, (union test_expectation_data){.int32 = actual}};
}

void uint32_callback(void* context, uint32_t actual) {
  assert_true(current().expectation == UINT32_EQ);
  assert_true(current().data.int32 == actual);
  current_expectation++;
}

void assert_uint64_eq(uint64_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      UINT64_EQ, (union test_expectation_data){.int64 = actual}};
}

void uint64_callback(void* context, uint64_t actual) {
  assert_true(current().expectation == UINT64_EQ);
  assert_true(current().data.int64 == actual);
  current_expectation++;
}

void assert_negint8_eq(uint8_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      NEGINT8_EQ, (union test_expectation_data){.int8 = actual}};
}

void negint8_callback(void* context, uint8_t actual) {
  assert_true(current().expectation == NEGINT8_EQ);
  assert_true(current().data.int8 == actual);
  current_expectation++;
}

void assert_negint16_eq(uint16_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      NEGINT16_EQ, (union test_expectation_data){.int16 = actual}};
}

void negint16_callback(void* context, uint16_t actual) {
  assert_true(current().expectation == NEGINT16_EQ);
  assert_true(current().data.int16 == actual);
  current_expectation++;
}

void assert_negint32_eq(uint32_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      NEGINT32_EQ, (union test_expectation_data){.int32 = actual}};
}

void negint32_callback(void* context, uint32_t actual) {
  assert_true(current().expectation == NEGINT32_EQ);
  assert_true(current().data.int32 == actual);
  current_expectation++;
}

void assert_negint64_eq(uint64_t actual) {
  assertions_queue[queue_size++] = (struct test_assertion){
      NEGINT64_EQ, (union test_expectation_data){.int64 = actual}};
}

void negint64_callback(void* context, uint64_t actual) {
  assert_true(current().expectation == NEGINT64_EQ);
  assert_true(current().data.int64 == actual);
  current_expectation++;
}

void assert_bstring_mem_eq(cbor_data address, size_t length) {
  assertions_queue[queue_size++] = (struct test_assertion){
      BSTRING_MEM_EQ,
      (union test_expectation_data){.string = {address, length}}};
}

void byte_string_callback(void* context, cbor_data address, size_t length) {
  assert_true(current().expectation == BSTRING_MEM_EQ);
  assert_true(current().data.string.address == address);
  assert_true(current().data.string.length == length);
  current_expectation++;
}

void assert_bstring_indef_start() {
  assertions_queue[queue_size++] =
      (struct test_assertion){.expectation = BSTRING_INDEF_START};
}

void byte_string_start_callback(void* context) {
  assert_true(current().expectation == BSTRING_INDEF_START);
  current_expectation++;
}

void assert_indef_break() {
  assertions_queue[queue_size++] =
      (struct test_assertion){.expectation = INDEF_BREAK};
}

void indef_break_callback(void* context) {
  assert_true(current().expectation == INDEF_BREAK);
  current_expectation++;
}

void assert_array_start(size_t length) {
  assertions_queue[queue_size++] =
      (struct test_assertion){ARRAY_START, {.length = length}};
}

void array_start_callback(void* context, size_t length) {
  assert_true(current().expectation == ARRAY_START);
  assert_true(current().data.length == length);
  current_expectation++;
}

void assert_indef_array_start() {
  assertions_queue[queue_size++] =
      (struct test_assertion){.expectation = ARRAY_INDEF_START};
}

void indef_array_start_callback(void* context) {
  assert_true(current().expectation == ARRAY_INDEF_START);
  current_expectation++;
}

void assert_map_start(size_t length) {
  assertions_queue[queue_size++] =
      (struct test_assertion){MAP_START, {.length = length}};
}

void map_start_callback(void* context, size_t length) {
  assert_true(current().expectation == MAP_START);
  assert_true(current().data.length == length);
  current_expectation++;
}

void assert_indef_map_start() {
  assertions_queue[queue_size++] =
      (struct test_assertion){.expectation = MAP_INDEF_START};
}

void indef_map_start_callback(void* context) {
  assert_true(current().expectation == MAP_INDEF_START);
  current_expectation++;
}

void assert_tag_eq(uint64_t value) {
  assertions_queue[queue_size++] =
      (struct test_assertion){TAG_EQ, {.int64 = value}};
}

void tag_callback(void* context, uint64_t value) {
  assert_true(current().expectation == TAG_EQ);
  assert_true(current().data.int64 == value);
  current_expectation++;
}

void assert_half(float value) {
  assertions_queue[queue_size++] =
      (struct test_assertion){HALF_EQ, {.float2 = value}};
}

void half_callback(void* context, float actual) {
  assert_true(current().expectation == HALF_EQ);
  assert_true(current().data.float2 == actual);
  current_expectation++;
}

void assert_float(float value) {
  assertions_queue[queue_size++] =
      (struct test_assertion){FLOAT_EQ, {.float4 = value}};
}

void float_callback(void* context, float actual) {
  assert_true(current().expectation == FLOAT_EQ);
  assert_true(current().data.float4 == actual);
  current_expectation++;
}

void assert_double(double value) {
  assertions_queue[queue_size++] =
      (struct test_assertion){DOUBLE_EQ, {.float8 = value}};
}

void double_callback(void* context, double actual) {
  assert_true(current().expectation == DOUBLE_EQ);
  assert_true(current().data.float8 == actual);
  current_expectation++;
}

void assert_bool(bool value) {
  assertions_queue[queue_size++] =
      (struct test_assertion){BOOL_EQ, {.boolean = value}};
}

void assert_nil() {
  assertions_queue[queue_size++] = (struct test_assertion){.expectation = NIL};
}

void assert_undef() {
  assertions_queue[queue_size++] =
      (struct test_assertion){.expectation = UNDEF};
}

void bool_callback(void* context, bool actual) {
  assert_true(current().expectation == BOOL_EQ);
  assert_true(current().data.boolean == actual);
  current_expectation++;
}

void null_callback(void* context) {
  assert_true(current().expectation == NIL);
  current_expectation++;
}

void undef_callback(void* context) {
  assert_true(current().expectation == UNDEF);
  current_expectation++;
}

const struct cbor_callbacks asserting_callbacks = {

    .uint8 = &uint8_callback,

    .uint16 = &uint16_callback,

    .uint32 = &uint32_callback,

    .uint64 = &uint64_callback,

    .negint8 = &negint8_callback,

    .negint16 = &negint16_callback,

    .negint32 = &negint32_callback,

    .negint64 = &negint64_callback,

    .byte_string = &byte_string_callback,
    .byte_string_start = &byte_string_start_callback,

    .array_start = &array_start_callback,
    .indef_array_start = &indef_array_start_callback,

    .map_start = &map_start_callback,
    .indef_map_start = &indef_map_start_callback,

    .tag = &tag_callback,

    .float2 = &half_callback,

    .float4 = &float_callback,

    .float8 = &double_callback,

    .undefined = &undef_callback,
    .boolean = &bool_callback,
    .null = &null_callback,
    .indef_break = &indef_break_callback};

struct cbor_decoder_result decode(cbor_data source, size_t source_size) {
  struct cbor_decoder_result result =
      decoder(source, source_size, &asserting_callbacks, NULL);
  /* Check remaining assertions */

  assert_true(current_expectation == queue_size);
  /* Clean up */
  current_expectation = queue_size = 0;
  return result;
}
