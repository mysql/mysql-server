#include <stdio.h>
#include <toku_assert.h>

int main(void) {
    int result = 42;
    assert_zero(result);
    return 0;
}
