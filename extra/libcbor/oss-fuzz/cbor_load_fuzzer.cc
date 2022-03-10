#include <cstdint>
#include <cstdio>

#include "cbor.h"

void *limited_malloc(size_t size) {
    if (size > 1 << 24) {
        return nullptr;
    }
    return malloc(size);
}

struct State {
    FILE* fout;

    State() : fout(fopen("/dev/null", "r")) {
        cbor_set_allocs(limited_malloc, realloc, free);
    }
};

static State kState;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    cbor_load_result result;
    cbor_item_t *item = cbor_load(Data, Size, &result);
    if (result.error.code == CBOR_ERR_NONE) {
        cbor_describe(item, kState.fout);
        unsigned char *buffer;
        size_t buffer_size;
        cbor_serialize_alloc(item, &buffer, &buffer_size);
        free(buffer);
        cbor_item_t *copied = cbor_copy(item);
        cbor_decref(&copied);
        cbor_decref(&item);
    }
    return 0;
}
