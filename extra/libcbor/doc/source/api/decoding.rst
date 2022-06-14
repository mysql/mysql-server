Decoding
=============================

The following diagram illustrates the relationship among different parts of libcbor from the decoding standpoint.

::

    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                              │
    │                                      Client application                                      │
    │                                                                                              │
    │                                                 ┌────────────────────────────────────────────┘
    │                                                 │                     ↕
    │                                                 │ ┌──────────────────────────────────────────┐
    │                                                 │ │                                          │
    │                                                 │ │          Manipulation routines           │
    │                                                 │ │                                          │
    │           ┌─────────────────────────────────────┘ └──────────────────────────────────────────┘
    │           │     ↑    ↑                  ↑                              ↑
    │           │     │    │    ┌─────────────╫──────────┬───────────────────┴─┐
    │           │     │   CDS   │             ║          │                     │
    │           │     │    │   PDS            ║         PDS                   PDS
    │           │     ↓    ↓    ↓             ↓          ↓                     ↓
    │           │ ┌─────────────────┐   ┌────────────────────┐   ┌────────────────────────────┐
    │           │ │                 │   │                    │   │                            │
    │           │ │  Custom driver  │ ↔ │  Streaming driver  │ ↔ │       Default driver       │ ↔ CD
    │           │ │                 │   │                    │   │                            │
    └───────────┘ └─────────────────┘   └────────────────────┘   └────────────────────────────┘
          ↕                ↕                        ↕                           ↕
    ┌──────────────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                              │
    │                            Stateless event─driven decoder                                    │
    │                                                                                              │
    └──────────────────────────────────────────────────────────────────────────────────────────────┘

                  (PSD = Provided Data Structures, CDS = Custom Data Structures)

This section will deal with the API that is labeled as the "Default driver" in the diagram. That is, routines that
decode complete libcbor data items

.. doxygenfunction:: cbor_load

Associated data structures
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. doxygenenum:: cbor_error_code

.. doxygenstruct:: cbor_load_result
    :members:

.. doxygenstruct:: cbor_error
    :members:

