{
  "type":"object",
  "required":["items","items2","items3","limit","offset","count", "links"],
  "properties": {
     "links" : {
       "type" : "array",
       "minItems":1,
       "maxItems":1,
       "Items": [{"type":"object", "required":["rel", "href"]}]},
     "count" : {"type" : "number", "minimum":3},
     "offset" : {"type" : "number", "minumum":0, "maximum":0},
     "limit" : {"type" : "number", "minumum":4294967295, "maximum":4294967295},
     "items" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]},
     "items2" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]},
     "items3" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]}
  }
}
