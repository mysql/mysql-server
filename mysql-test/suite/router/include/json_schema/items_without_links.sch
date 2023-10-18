{
  "type":"object",
  "required":["items","limit","offset","count"],
  "not": { "required": [ "links" ] },
  "properties": {
     "count" : {"type" : "number", "minimum":1},
     "offset" : {"type" : "number", "minumum":0},
     "limit" : {"type" : "number", "minumum":1},
     "items" : {
        "type" : "array",
        "minItems":1,
        "Items": [{"type":"object"}]}
  }
}
