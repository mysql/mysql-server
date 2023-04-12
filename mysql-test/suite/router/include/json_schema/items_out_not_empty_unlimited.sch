{
  "type":"object",
  "required":["items","itemsOut","limit","offset","count", "links"],
  "properties": {
     "links" : {
       "type" : "array",
       "minItems":1,
       "maxItems":1,
       "Items": [{"type":"object", "required":["rel", "href"]}]},
     "count" : {"type" : "number", "minimum":2},
     "offset" : {"type" : "number", "minumum":0, "maximum":0},
     "limit" : {"type" : "number", "minumum":4294967295, "maximum":4294967295},
     "items" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]},
     "itemsOut" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]}
  }
}
