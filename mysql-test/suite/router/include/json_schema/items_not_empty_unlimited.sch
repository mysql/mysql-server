{
  "type":"object",
  "required":["items"],
  "properties": {
     "items" : {
        "type" : "array",
        "minItems":1,
        "Items": [
           {
             "type":"object",
             "required":["items", "_metadata"],
             "properties": {
               "items" : {
                 "type" : "array",
                 "minItems":1
               }
             }
           }]}
  }
}
