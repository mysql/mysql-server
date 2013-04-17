#include <stdio.h>
#include <assert.h>
#include <toku_stdint.h>
#include <toku_os.h>

int main(void) {
    uint64_t cpuhz;
    int r = toku_os_get_processor_frequency(&cpuhz);
    assert(r == 0);
    printf("%"PRIu64"\n", cpuhz);
    return 0;
}
