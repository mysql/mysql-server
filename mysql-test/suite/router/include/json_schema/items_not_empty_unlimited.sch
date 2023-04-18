{
  "type":"object",
  "required":["items","count", "links"],
  "properties": {
     "links" : {
       "type" : "array",
       "minItems":1,
       "maxItems":1,
       "Items": [{"type":"object", "required":["rel", "href"]}]},
     "count" : {"type" : "number", "minimum":26},
     "items" : {
        "type" : "array",
        "minItems":26,
        "Items": [{"type":"object"}]}
  }
}
