{
  "type":"object",
  "required":["items","itemsOut","count", "links"],
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
     "itemsOut" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]}
  }
}
