{
  "type":"object",
  "required":["itemsOut","count", "links"],
  "properties": {
     "links" : {
       "type" : "array",
       "minItems":1,
       "maxItems":1,
       "Items": [{"type":"object", "required":["rel", "href"]}]},
     "count" : {"type" : "number", "minimum":1},
     "itemsOut" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]}
  }
}
