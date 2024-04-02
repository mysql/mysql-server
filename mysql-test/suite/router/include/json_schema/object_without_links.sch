{
  "type":"object",
  "required":["_metadata"],
  "not": { "required": [ "links" ] },
  "properties": {
     "_metadata" : {
        "type" : "object",
         "required":["etag"]}
  }
}
