{
  "type":"object",
  "required":[
      "_metadata",
      "id",
      "date",
      "name",
      "links"
   ],
  "properties": {
     "comment": {"type": "string"},
     "date": {"type": "string"},
     "name": {"type": "string"},
     "id": {"type": "integer"},
     "links" : {
        "type" : "array"
     },
     "_metadata" : {
        "type" : "object",
        "required":["etag"]
    }
  }
}