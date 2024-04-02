{
  "type":"object",
  "required":["items","items2","count", "links"],
  "properties": {
     "links" : {
       "type" : "array",
       "minItems":1,
       "maxItems":1,
       "Items": [{"type":"object", "required":["rel", "href"]}]},
     "count" : {"type" : "number", "minimum":2},
     "items" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]},
     "items2" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]}
  }
}
