Streaming & indefinite items
=============================

CBOR :doc:`strings <api/type_2>`, :doc:`byte strings <api/type_3>`, :doc:`arrays <api/type_4>`, and :doc:`maps <api/type_5>` can be encoded as *indefinite*, meaning their length or size is not specified. Instead, they are divided into *chunks* (:doc:`strings <api/type_2>`, :doc:`byte strings <api/type_3>`), or explicitly terminated (:doc:`arrays <api/type_4>`, :doc:`maps <api/type_5>`).

This is one of the most important (and due to poor implementations, underutilized) features of CBOR. It enables low-overhead streaming just about anywhere without dealing with channels or pub/sub mechanism. It is, however, important to recognize that CBOR streaming is not a substitute for  Websockets [#]_ and similar technologies.

.. [#] :RFC:`6455`

.. toctree::

   streaming/decoding
   streaming/encoding
