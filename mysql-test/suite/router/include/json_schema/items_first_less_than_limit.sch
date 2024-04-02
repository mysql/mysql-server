{
  "type":"object",
  "required":["items","limit","offset","count", "links"],
  "properties": {
     "links" : {
       "type" : "array",
       "minItems":1,
       "maxItems":1,
       "Items": [{"type":"object", "required":["rel", "href"]}]},
     "count" : {"type" : "number", "minimum":1},
     "offset" : {"type" : "number", "minumum":0, "maximum":0},
     "limit" : {"type" : "number", "minumum":25, "maximum":25},
     "items" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]}
  }
}
