Usage & preliminaries
=======================

Version information
--------------------

libcbor exports its version using three self-explanatory macros:

 - ``CBOR_MAJOR_VERSION``
 - ``CBOR_MINOR_VERSION``
 - ``CBOR_PATCH_VERSION``

The ``CBOR_VERSION`` is a string concatenating these three identifiers into one (e.g. ``0.2.0``).

In order to simplify version comparisons, the version is also exported as

.. code-block:: c

  #define CBOR_HEX_VERSION ((CBOR_MAJOR_VERSION << 16) | (CBOR_MINOR_VERSION << 8) | CBOR_PATCH_VERSION)

Since macros are difficult to work with through FFIs, the same information is also available through three ``uint8_t`` constants,
namely

 - ``cbor_major_version``
 - ``cbor_minor_version``
 - ``cbor_patch_version``


Headers to include
---------------------

The ``cbor.h`` header includes all the symbols. If, for any reason, you don't want to include all the exported symbols,
feel free to use just some of the ``cbor/*.h`` headers:

 - ``cbor/arrays.h`` - :doc:`api/type_4`
 - ``cbor/bytestrings.h`` - :doc:`api/type_2`
 - ``cbor/callbacks.h`` - Callbacks used for :doc:`streaming/decoding`
 - ``cbor/common.h`` - Common utilities - always transitively included
 - ``cbor/data.h`` - Data types definitions - always transitively included
 - ``cbor/encoding.h`` - Streaming encoders for :doc:`streaming/encoding`
 - ``cbor/floats_ctrls.h`` - :doc:`api/type_7`
 - ``cbor/ints.h`` - :doc:`api/type_0_1`
 - ``cbor/maps.h`` - :doc:`api/type_5`
 - ``cbor/serialization.h`` - High level serialization such as :func:`cbor_serialize`
 - ``cbor/streaming.h`` - Home of :func:`cbor_stream_decode`
 - ``cbor/strings.h`` - :doc:`api/type_3`
 - ``cbor/tags.h`` - :doc:`api/type_6`


Using libcbor
--------------

If you want to get more familiar with CBOR, we recommend the `cbor.io <http://cbor.io/>`_ website. Once you get the grasp
of what is it CBOR does, the examples (located in the ``examples`` directory) should give you a good feel of the API. The
:doc:`API documentation <api>` should then provide with all the information you may need.


**Creating and serializing items**

.. code-block:: c

    #include "cbor.h"
    #include <stdio.h>

    int main(int argc, char * argv[])
    {
        /* Preallocate the map structure */
        cbor_item_t * root = cbor_new_definite_map(2);
        /* Add the content */
        cbor_map_add(root, (struct cbor_pair) {
            .key = cbor_move(cbor_build_string("Is CBOR awesome?")),
            .value = cbor_move(cbor_build_bool(true))
        });
        cbor_map_add(root, (struct cbor_pair) {
            .key = cbor_move(cbor_build_uint8(42)),
            .value = cbor_move(cbor_build_string("Is the answer"))
        });
        /* Output: `length` bytes of data in the `buffer` */
        unsigned char * buffer;
        size_t buffer_size, length = cbor_serialize_alloc(root, &buffer, &buffer_size);

        fwrite(buffer, 1, length, stdout);
        free(buffer);

        fflush(stdout);
        cbor_decref(&root);
    }


**Reading serialized data**

.. code-block:: c

    #include "cbor.h"
    #include <stdio.h>

    /*
     * Reads data from a file. Example usage:
     * $ ./examples/readfile examples/data/nested_array.cbor
     */

    int main(int argc, char * argv[])
    {
        FILE * f = fopen(argv[1], "rb");
        fseek(f, 0, SEEK_END);
        size_t length = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char * buffer = malloc(length);
        fread(buffer, length, 1, f);

        /* Assuming `buffer` contains `info.st_size` bytes of input data */
        struct cbor_load_result result;
        cbor_item_t * item = cbor_load(buffer, length, &result);
        /* Pretty-print the result */
        cbor_describe(item, stdout);
        fflush(stdout);
        /* Deallocate the result */
        cbor_decref(&item);

        fclose(f);
    }


**Using the streaming parser**

.. code-block:: c

    #include "cbor.h"
    #include <stdio.h>
    #include <string.h>

    /*
     * Illustrates how one might skim through a map (which is assumed to have
     * string keys and values only), looking for the value of a specific key
     *
     * Use the examples/data/map.cbor input to test this.
     */

    const char * key = "a secret key";
    bool key_found = false;

    void find_string(void * _ctx, cbor_data buffer, size_t len)
    {
        if (key_found) {
            printf("Found the value: %*s\n", (int) len, buffer);
            key_found = false;
        } else if (len == strlen(key)) {
            key_found = (memcmp(key, buffer, len) == 0);
        }
    }

    int main(int argc, char * argv[])
    {
        FILE * f = fopen(argv[1], "rb");
        fseek(f, 0, SEEK_END);
        size_t length = (size_t)ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char * buffer = malloc(length);
        fread(buffer, length, 1, f);

        struct cbor_callbacks callbacks = cbor_empty_callbacks;
        struct cbor_decoder_result decode_result;
        size_t bytes_read = 0;
        callbacks.string = find_string;
        while (bytes_read < length) {
            decode_result = cbor_stream_decode(buffer + bytes_read,
                                               length - bytes_read,
                                               &callbacks, NULL);
            bytes_read += decode_result.read;
        }

        fclose(f);
    }
