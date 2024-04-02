{
  "type":"object",
  "required":["items","items2","items3","count", "links"],
  "properties": {
     "links" : {
       "type" : "array",
       "minItems":1,
       "maxItems":1,
       "Items": [{"type":"object", "required":["rel", "href"]}]},
     "count" : {"type" : "number", "minimum":3},
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
