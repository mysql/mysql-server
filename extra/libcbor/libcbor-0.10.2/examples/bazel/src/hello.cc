#include "src/hello.h"

#include "cbor.h"

void print_cbor_version() {
  printf("libcbor v%d.%d.%d\n", cbor_major_version, cbor_minor_version, cbor_patch_version);
}
