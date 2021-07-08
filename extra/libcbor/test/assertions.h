#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include <cmocka.h>

#include "cbor.h"

#ifndef ASSERTIONS_H_
#define ASSERTIONS_H_

void assert_uint8(cbor_item_t* item, uint8_t num);
void assert_uint16(cbor_item_t* item, uint16_t num);
void assert_uint32(cbor_item_t* item, uint32_t num);
void assert_uint64(cbor_item_t* item, uint64_t num);

void assert_decoder_result(size_t, enum cbor_decoder_status,
                           struct cbor_decoder_result);
void assert_decoder_result_nedata(size_t, struct cbor_decoder_result);

#endif
