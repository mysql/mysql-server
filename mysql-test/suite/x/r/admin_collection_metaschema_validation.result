create schema xtest default charset 'utf8mb4';


## I. Test validation of a user provided schema encoded as
##    protobuf.string and a Datatypes.Object
#
# 1. Unknown property.
# 2. Mismatch "id" property type.
# 3. Mismatch "schema" property type.
# 4. Mismatch "title" property type.
# 5. Mismatch "description" property type.
# 6. Mismatch "multipleOf" property type.
# 7. Wrong "multipleOf" property value.
# 8. Mismatch "maximum" property type.
# 9. Mismatch "exclusiveMaximum" property type.
# 10. Mismatch "minimum" property type.
# 11. Mismatch "exclusiveminimum" property type.
# 12. Mismatch "maxLength" property type.
# 13. Wrong "maxLength" property value.
# 14. Mismatch "minLength" property type.
# 15. Wrong "minLength" property value.
# 16. Mismatch "pattern" property type.
# 17. Mismatch "additionalItems" property type.
# 18. Invalid schema contained in the "additionalItems" property.
# 19. Empty "items" properties array.
# 20. "items" properties containing invalid schema.
# 21. "items" properties array containing invalid schema.
# 22. Mismatch "maxItems" properties type.
# 23. Wrong "maxItems" properties value.
# 24. Mismatch "minItems" properties type.
# 25. Wrong "minItems" properties value.
# 26. Mismatch "uniqueItems" properties type.
# 27. Mismatch "maxProperties" properties type.
# 28. Wrong "maxProperties" properties value.
# 29. Mismatch "minProperties" properties type.
# 30. Wrong "minProperties" properties value.
# 31. Mismatch "required" properties type.
# 32. Empty "required" property field.
# 33. "required" property field containing an array of invalid types.
# 34. "required" properties array containing non-unique items.
# 35. Mismatch "additionalProperties" property type.
# 36. Invalid schema in the "additionalProperties" property field.
# 37. Mismatch "definitions" property type.
# 38. "definitions" property containing an invalid schema.
# 39. Mismatch "properties" property type.
# 40. "properties" property containing an invalid schema.
# 41. Mismatch "patternProperties" property type.
# 42. "patternProperties" property containing an invalid schema.
# 43. Mismatch "dependencies" property type.
# 44. "dependencies" properties containing an invalid schema.
# 45. "dependencies" properties containing an empty array.
# 46. "dependencies" properties containing an array of invalid types.
# 47. "dependencies" properties containing an array of a non-unique types.
# 48. Mismatch "enum" property type.
# 49. Empty "enum" properties array.
# 50. Non-unique values in the "enum" property array.
# 51. Mismatch "type" property type.
# 52. Invalid type in "type" properties array.
# 53. Empty "type" properties array.
# 54. Non-unique "type" properties array.
# 55. Mismatch "format" property type.
# 56. Mismatch "allOf" property type.
# 57. Empty "allOf" properties array.
# 58. Invalid schema in the "allOf" properties array.
# 59. Mismatch "anyOf" property type.
# 60. Empty "anyOf" properties array.
# 61. Invalid schema in the "anyOf" properties array.
# 62. Mismatch "oneOf" property type.
# 63. Empty "oneOf" properties array.
# 64. Invalid schema in the "oneOf" properties array.
# 65. Invalid schema as the "not" property value.

#
# I.1
Got expected error: JSON validation schema location #/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.2
Got expected error: JSON validation schema location #/id failed requirement: 'type' at meta schema location '#/properties/id' (code 5182)
Got expected error: JSON validation schema location #/id failed requirement: 'type' at meta schema location '#/properties/id' (code 5182)

#
# I.3
Got expected error: JSON validation schema location #/%24schema failed requirement: 'type' at meta schema location '#/properties/%24schema' (code 5182)
Got expected error: JSON validation schema location #/%24schema failed requirement: 'type' at meta schema location '#/properties/%24schema' (code 5182)

#
# I.4
Got expected error: JSON validation schema location #/title failed requirement: 'type' at meta schema location '#/properties/title' (code 5182)
Got expected error: JSON validation schema location #/title failed requirement: 'type' at meta schema location '#/properties/title' (code 5182)

#
# I.5
Got expected error: JSON validation schema location #/description failed requirement: 'type' at meta schema location '#/properties/description' (code 5182)
Got expected error: JSON validation schema location #/description failed requirement: 'type' at meta schema location '#/properties/description' (code 5182)

#
# I.6
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'type' at meta schema location '#/properties/multipleOf' (code 5182)
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'type' at meta schema location '#/properties/multipleOf' (code 5182)

#
# I.7
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'minimum' at meta schema location '#/properties/multipleOf' (code 5182)
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'minimum' at meta schema location '#/properties/multipleOf' (code 5182)

#
# I.8
Got expected error: JSON validation schema location #/maximum failed requirement: 'type' at meta schema location '#/properties/maximum' (code 5182)
Got expected error: JSON validation schema location #/maximum failed requirement: 'type' at meta schema location '#/properties/maximum' (code 5182)

#
# I.9
Got expected error: JSON validation schema location #/exclusiveMaximum failed requirement: 'type' at meta schema location '#/properties/exclusiveMaximum' (code 5182)
Got expected error: JSON validation schema location #/exclusiveMaximum failed requirement: 'type' at meta schema location '#/properties/exclusiveMaximum' (code 5182)

#
# I.10
Got expected error: JSON validation schema location #/minimum failed requirement: 'type' at meta schema location '#/properties/minimum' (code 5182)
Got expected error: JSON validation schema location #/minimum failed requirement: 'type' at meta schema location '#/properties/minimum' (code 5182)

#
# I.11
Got expected error: JSON validation schema location #/exclusiveMinimum failed requirement: 'type' at meta schema location '#/properties/exclusiveMinimum' (code 5182)
Got expected error: JSON validation schema location #/exclusiveMinimum failed requirement: 'type' at meta schema location '#/properties/exclusiveMinimum' (code 5182)

#
# I.12
Got expected error: JSON validation schema location #/maxLength failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxLength failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.13
Got expected error: JSON validation schema location #/maxLength failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxLength failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.14
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.15
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.16
Got expected error: JSON validation schema location #/pattern failed requirement: 'type' at meta schema location '#/properties/pattern' (code 5182)
Got expected error: JSON validation schema location #/pattern failed requirement: 'type' at meta schema location '#/properties/pattern' (code 5182)

#
# I.17
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)

#
# I.18
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)

#
# I.19
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.20
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.21
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.22
Got expected error: JSON validation schema location #/maxItems failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxItems failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.23
Got expected error: JSON validation schema location #/maxItems failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxItems failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.24
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.25
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.26
Got expected error: JSON validation schema location #/uniqueItems failed requirement: 'type' at meta schema location '#/properties/uniqueItems' (code 5182)
Got expected error: JSON validation schema location #/uniqueItems failed requirement: 'type' at meta schema location '#/properties/uniqueItems' (code 5182)

#
# I.27
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.28
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.29
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.30
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.31
Got expected error: JSON validation schema location #/required failed requirement: 'type' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required failed requirement: 'type' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.32
Got expected error: JSON validation schema location #/required failed requirement: 'minItems' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required failed requirement: 'minItems' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.33
Got expected error: JSON validation schema location #/required/0 failed requirement: 'type' at meta schema location '#/definitions/stringArray/items' (code 5182)
Got expected error: JSON validation schema location #/required/0 failed requirement: 'type' at meta schema location '#/definitions/stringArray/items' (code 5182)

#
# I.34
Got expected error: JSON validation schema location #/required/1 failed requirement: 'uniqueItems' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required/1 failed requirement: 'uniqueItems' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.35
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)

#
# I.36
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)

#
# I.37
Got expected error: JSON validation schema location #/definitions failed requirement: 'type' at meta schema location '#/properties/definitions' (code 5182)
Got expected error: JSON validation schema location #/definitions failed requirement: 'type' at meta schema location '#/properties/definitions' (code 5182)

#
# I.38
Got expected error: JSON validation schema location #/definitions/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/definitions/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.39
Got expected error: JSON validation schema location #/properties failed requirement: 'type' at meta schema location '#/properties/properties' (code 5182)
Got expected error: JSON validation schema location #/properties failed requirement: 'type' at meta schema location '#/properties/properties' (code 5182)

#
# I.40
Got expected error: JSON validation schema location #/properties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/properties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.41
Got expected error: JSON validation schema location #/patternProperties failed requirement: 'type' at meta schema location '#/properties/patternProperties' (code 5182)
Got expected error: JSON validation schema location #/patternProperties failed requirement: 'type' at meta schema location '#/properties/patternProperties' (code 5182)

#
# I.42
Got expected error: JSON validation schema location #/patternProperties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/patternProperties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.43
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.44
Got expected error: JSON validation schema location #/dependencies/unknown failed requirement: 'anyOf' at meta schema location '#/properties/dependencies/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/dependencies/unknown failed requirement: 'anyOf' at meta schema location '#/properties/dependencies/additionalProperties' (code 5182)

#
# I.45
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.46
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.47
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.48
Got expected error: JSON validation schema location #/enum failed requirement: 'type' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum failed requirement: 'type' at meta schema location '#/properties/enum' (code 5182)

#
# I.49
Got expected error: JSON validation schema location #/enum failed requirement: 'minItems' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum failed requirement: 'minItems' at meta schema location '#/properties/enum' (code 5182)

#
# I.50
Got expected error: JSON validation schema location #/enum/1 failed requirement: 'uniqueItems' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum/1 failed requirement: 'uniqueItems' at meta schema location '#/properties/enum' (code 5182)

#
# I.51
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.52
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.53
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.54
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.55
Got expected error: JSON validation schema location #/format failed requirement: 'type' at meta schema location '#/properties/format' (code 5182)
Got expected error: JSON validation schema location #/format failed requirement: 'type' at meta schema location '#/properties/format' (code 5182)

#
# I.56
Got expected error: JSON validation schema location #/allOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/allOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.57
Got expected error: JSON validation schema location #/allOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/allOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.58
Got expected error: JSON validation schema location #/allOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/allOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.59
Got expected error: JSON validation schema location #/anyOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/anyOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.60
Got expected error: JSON validation schema location #/anyOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/anyOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.61
Got expected error: JSON validation schema location #/anyOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/anyOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.62
Got expected error: JSON validation schema location #/oneOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/oneOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.63
Got expected error: JSON validation schema location #/oneOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/oneOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.64
Got expected error: JSON validation schema location #/oneOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/oneOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.65
Got expected error: JSON validation schema location #/not failed requirement: 'type' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/not failed requirement: 'type' at meta schema location '#' (code 5182)
Mysqlx.Ok {
  msg: "bye!"
}
ok


## I. Test validation of a user provided schema encoded as
##    protobuf.string and a Datatypes.Object
#
# 1. Unknown property.
# 2. Mismatch "id" property type.
# 3. Mismatch "schema" property type.
# 4. Mismatch "title" property type.
# 5. Mismatch "description" property type.
# 6. Mismatch "multipleOf" property type.
# 7. Wrong "multipleOf" property value.
# 8. Mismatch "maximum" property type.
# 9. Mismatch "exclusiveMaximum" property type.
# 10. Mismatch "minimum" property type.
# 11. Mismatch "exclusiveminimum" property type.
# 12. Mismatch "maxLength" property type.
# 13. Wrong "maxLength" property value.
# 14. Mismatch "minLength" property type.
# 15. Wrong "minLength" property value.
# 16. Mismatch "pattern" property type.
# 17. Mismatch "additionalItems" property type.
# 18. Invalid schema contained in the "additionalItems" property.
# 19. Empty "items" properties array.
# 20. "items" properties containing invalid schema.
# 21. "items" properties array containing invalid schema.
# 22. Mismatch "maxItems" properties type.
# 23. Wrong "maxItems" properties value.
# 24. Mismatch "minItems" properties type.
# 25. Wrong "minItems" properties value.
# 26. Mismatch "uniqueItems" properties type.
# 27. Mismatch "maxProperties" properties type.
# 28. Wrong "maxProperties" properties value.
# 29. Mismatch "minProperties" properties type.
# 30. Wrong "minProperties" properties value.
# 31. Mismatch "required" properties type.
# 32. Empty "required" property field.
# 33. "required" property field containing an array of invalid types.
# 34. "required" properties array containing non-unique items.
# 35. Mismatch "additionalProperties" property type.
# 36. Invalid schema in the "additionalProperties" property field.
# 37. Mismatch "definitions" property type.
# 38. "definitions" property containing an invalid schema.
# 39. Mismatch "properties" property type.
# 40. "properties" property containing an invalid schema.
# 41. Mismatch "patternProperties" property type.
# 42. "patternProperties" property containing an invalid schema.
# 43. Mismatch "dependencies" property type.
# 44. "dependencies" properties containing an invalid schema.
# 45. "dependencies" properties containing an empty array.
# 46. "dependencies" properties containing an array of invalid types.
# 47. "dependencies" properties containing an array of a non-unique types.
# 48. Mismatch "enum" property type.
# 49. Empty "enum" properties array.
# 50. Non-unique values in the "enum" property array.
# 51. Mismatch "type" property type.
# 52. Invalid type in "type" properties array.
# 53. Empty "type" properties array.
# 54. Non-unique "type" properties array.
# 55. Mismatch "format" property type.
# 56. Mismatch "allOf" property type.
# 57. Empty "allOf" properties array.
# 58. Invalid schema in the "allOf" properties array.
# 59. Mismatch "anyOf" property type.
# 60. Empty "anyOf" properties array.
# 61. Invalid schema in the "anyOf" properties array.
# 62. Mismatch "oneOf" property type.
# 63. Empty "oneOf" properties array.
# 64. Invalid schema in the "oneOf" properties array.
# 65. Invalid schema as the "not" property value.

#
# I.1
Got expected error: JSON validation schema location #/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.2
Got expected error: JSON validation schema location #/id failed requirement: 'type' at meta schema location '#/properties/id' (code 5182)
Got expected error: JSON validation schema location #/id failed requirement: 'type' at meta schema location '#/properties/id' (code 5182)

#
# I.3
Got expected error: JSON validation schema location #/%24schema failed requirement: 'type' at meta schema location '#/properties/%24schema' (code 5182)
Got expected error: JSON validation schema location #/%24schema failed requirement: 'type' at meta schema location '#/properties/%24schema' (code 5182)

#
# I.4
Got expected error: JSON validation schema location #/title failed requirement: 'type' at meta schema location '#/properties/title' (code 5182)
Got expected error: JSON validation schema location #/title failed requirement: 'type' at meta schema location '#/properties/title' (code 5182)

#
# I.5
Got expected error: JSON validation schema location #/description failed requirement: 'type' at meta schema location '#/properties/description' (code 5182)
Got expected error: JSON validation schema location #/description failed requirement: 'type' at meta schema location '#/properties/description' (code 5182)

#
# I.6
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'type' at meta schema location '#/properties/multipleOf' (code 5182)
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'type' at meta schema location '#/properties/multipleOf' (code 5182)

#
# I.7
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'minimum' at meta schema location '#/properties/multipleOf' (code 5182)
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'minimum' at meta schema location '#/properties/multipleOf' (code 5182)

#
# I.8
Got expected error: JSON validation schema location #/maximum failed requirement: 'type' at meta schema location '#/properties/maximum' (code 5182)
Got expected error: JSON validation schema location #/maximum failed requirement: 'type' at meta schema location '#/properties/maximum' (code 5182)

#
# I.9
Got expected error: JSON validation schema location #/exclusiveMaximum failed requirement: 'type' at meta schema location '#/properties/exclusiveMaximum' (code 5182)
Got expected error: JSON validation schema location #/exclusiveMaximum failed requirement: 'type' at meta schema location '#/properties/exclusiveMaximum' (code 5182)

#
# I.10
Got expected error: JSON validation schema location #/minimum failed requirement: 'type' at meta schema location '#/properties/minimum' (code 5182)
Got expected error: JSON validation schema location #/minimum failed requirement: 'type' at meta schema location '#/properties/minimum' (code 5182)

#
# I.11
Got expected error: JSON validation schema location #/exclusiveMinimum failed requirement: 'type' at meta schema location '#/properties/exclusiveMinimum' (code 5182)
Got expected error: JSON validation schema location #/exclusiveMinimum failed requirement: 'type' at meta schema location '#/properties/exclusiveMinimum' (code 5182)

#
# I.12
Got expected error: JSON validation schema location #/maxLength failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxLength failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.13
Got expected error: JSON validation schema location #/maxLength failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxLength failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.14
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.15
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.16
Got expected error: JSON validation schema location #/pattern failed requirement: 'type' at meta schema location '#/properties/pattern' (code 5182)
Got expected error: JSON validation schema location #/pattern failed requirement: 'type' at meta schema location '#/properties/pattern' (code 5182)

#
# I.17
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)

#
# I.18
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)

#
# I.19
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.20
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.21
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.22
Got expected error: JSON validation schema location #/maxItems failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxItems failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.23
Got expected error: JSON validation schema location #/maxItems failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxItems failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.24
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.25
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.26
Got expected error: JSON validation schema location #/uniqueItems failed requirement: 'type' at meta schema location '#/properties/uniqueItems' (code 5182)
Got expected error: JSON validation schema location #/uniqueItems failed requirement: 'type' at meta schema location '#/properties/uniqueItems' (code 5182)

#
# I.27
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.28
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.29
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.30
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.31
Got expected error: JSON validation schema location #/required failed requirement: 'type' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required failed requirement: 'type' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.32
Got expected error: JSON validation schema location #/required failed requirement: 'minItems' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required failed requirement: 'minItems' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.33
Got expected error: JSON validation schema location #/required/0 failed requirement: 'type' at meta schema location '#/definitions/stringArray/items' (code 5182)
Got expected error: JSON validation schema location #/required/0 failed requirement: 'type' at meta schema location '#/definitions/stringArray/items' (code 5182)

#
# I.34
Got expected error: JSON validation schema location #/required/1 failed requirement: 'uniqueItems' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required/1 failed requirement: 'uniqueItems' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.35
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)

#
# I.36
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)

#
# I.37
Got expected error: JSON validation schema location #/definitions failed requirement: 'type' at meta schema location '#/properties/definitions' (code 5182)
Got expected error: JSON validation schema location #/definitions failed requirement: 'type' at meta schema location '#/properties/definitions' (code 5182)

#
# I.38
Got expected error: JSON validation schema location #/definitions/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/definitions/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.39
Got expected error: JSON validation schema location #/properties failed requirement: 'type' at meta schema location '#/properties/properties' (code 5182)
Got expected error: JSON validation schema location #/properties failed requirement: 'type' at meta schema location '#/properties/properties' (code 5182)

#
# I.40
Got expected error: JSON validation schema location #/properties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/properties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.41
Got expected error: JSON validation schema location #/patternProperties failed requirement: 'type' at meta schema location '#/properties/patternProperties' (code 5182)
Got expected error: JSON validation schema location #/patternProperties failed requirement: 'type' at meta schema location '#/properties/patternProperties' (code 5182)

#
# I.42
Got expected error: JSON validation schema location #/patternProperties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/patternProperties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.43
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.44
Got expected error: JSON validation schema location #/dependencies/unknown failed requirement: 'anyOf' at meta schema location '#/properties/dependencies/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/dependencies/unknown failed requirement: 'anyOf' at meta schema location '#/properties/dependencies/additionalProperties' (code 5182)

#
# I.45
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.46
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.47
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.48
Got expected error: JSON validation schema location #/enum failed requirement: 'type' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum failed requirement: 'type' at meta schema location '#/properties/enum' (code 5182)

#
# I.49
Got expected error: JSON validation schema location #/enum failed requirement: 'minItems' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum failed requirement: 'minItems' at meta schema location '#/properties/enum' (code 5182)

#
# I.50
Got expected error: JSON validation schema location #/enum/1 failed requirement: 'uniqueItems' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum/1 failed requirement: 'uniqueItems' at meta schema location '#/properties/enum' (code 5182)

#
# I.51
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.52
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.53
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.54
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.55
Got expected error: JSON validation schema location #/format failed requirement: 'type' at meta schema location '#/properties/format' (code 5182)
Got expected error: JSON validation schema location #/format failed requirement: 'type' at meta schema location '#/properties/format' (code 5182)

#
# I.56
Got expected error: JSON validation schema location #/allOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/allOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.57
Got expected error: JSON validation schema location #/allOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/allOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.58
Got expected error: JSON validation schema location #/allOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/allOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.59
Got expected error: JSON validation schema location #/anyOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/anyOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.60
Got expected error: JSON validation schema location #/anyOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/anyOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.61
Got expected error: JSON validation schema location #/anyOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/anyOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.62
Got expected error: JSON validation schema location #/oneOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/oneOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.63
Got expected error: JSON validation schema location #/oneOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/oneOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.64
Got expected error: JSON validation schema location #/oneOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/oneOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.65
Got expected error: JSON validation schema location #/not failed requirement: 'type' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/not failed requirement: 'type' at meta schema location '#' (code 5182)
Mysqlx.Ok {
  msg: "bye!"
}
ok


## I. Test validation of a user provided schema encoded as
##    protobuf.string and a Datatypes.Object
#
# 1. Unknown property.
# 2. Mismatch "id" property type.
# 3. Mismatch "schema" property type.
# 4. Mismatch "title" property type.
# 5. Mismatch "description" property type.
# 6. Mismatch "multipleOf" property type.
# 7. Wrong "multipleOf" property value.
# 8. Mismatch "maximum" property type.
# 9. Mismatch "exclusiveMaximum" property type.
# 10. Mismatch "minimum" property type.
# 11. Mismatch "exclusiveminimum" property type.
# 12. Mismatch "maxLength" property type.
# 13. Wrong "maxLength" property value.
# 14. Mismatch "minLength" property type.
# 15. Wrong "minLength" property value.
# 16. Mismatch "pattern" property type.
# 17. Mismatch "additionalItems" property type.
# 18. Invalid schema contained in the "additionalItems" property.
# 19. Empty "items" properties array.
# 20. "items" properties containing invalid schema.
# 21. "items" properties array containing invalid schema.
# 22. Mismatch "maxItems" properties type.
# 23. Wrong "maxItems" properties value.
# 24. Mismatch "minItems" properties type.
# 25. Wrong "minItems" properties value.
# 26. Mismatch "uniqueItems" properties type.
# 27. Mismatch "maxProperties" properties type.
# 28. Wrong "maxProperties" properties value.
# 29. Mismatch "minProperties" properties type.
# 30. Wrong "minProperties" properties value.
# 31. Mismatch "required" properties type.
# 32. Empty "required" property field.
# 33. "required" property field containing an array of invalid types.
# 34. "required" properties array containing non-unique items.
# 35. Mismatch "additionalProperties" property type.
# 36. Invalid schema in the "additionalProperties" property field.
# 37. Mismatch "definitions" property type.
# 38. "definitions" property containing an invalid schema.
# 39. Mismatch "properties" property type.
# 40. "properties" property containing an invalid schema.
# 41. Mismatch "patternProperties" property type.
# 42. "patternProperties" property containing an invalid schema.
# 43. Mismatch "dependencies" property type.
# 44. "dependencies" properties containing an invalid schema.
# 45. "dependencies" properties containing an empty array.
# 46. "dependencies" properties containing an array of invalid types.
# 47. "dependencies" properties containing an array of a non-unique types.
# 48. Mismatch "enum" property type.
# 49. Empty "enum" properties array.
# 50. Non-unique values in the "enum" property array.
# 51. Mismatch "type" property type.
# 52. Invalid type in "type" properties array.
# 53. Empty "type" properties array.
# 54. Non-unique "type" properties array.
# 55. Mismatch "format" property type.
# 56. Mismatch "allOf" property type.
# 57. Empty "allOf" properties array.
# 58. Invalid schema in the "allOf" properties array.
# 59. Mismatch "anyOf" property type.
# 60. Empty "anyOf" properties array.
# 61. Invalid schema in the "anyOf" properties array.
# 62. Mismatch "oneOf" property type.
# 63. Empty "oneOf" properties array.
# 64. Invalid schema in the "oneOf" properties array.
# 65. Invalid schema as the "not" property value.

#
# I.1
Got expected error: JSON validation schema location #/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.2
Got expected error: JSON validation schema location #/id failed requirement: 'type' at meta schema location '#/properties/id' (code 5182)
Got expected error: JSON validation schema location #/id failed requirement: 'type' at meta schema location '#/properties/id' (code 5182)

#
# I.3
Got expected error: JSON validation schema location #/%24schema failed requirement: 'type' at meta schema location '#/properties/%24schema' (code 5182)
Got expected error: JSON validation schema location #/%24schema failed requirement: 'type' at meta schema location '#/properties/%24schema' (code 5182)

#
# I.4
Got expected error: JSON validation schema location #/title failed requirement: 'type' at meta schema location '#/properties/title' (code 5182)
Got expected error: JSON validation schema location #/title failed requirement: 'type' at meta schema location '#/properties/title' (code 5182)

#
# I.5
Got expected error: JSON validation schema location #/description failed requirement: 'type' at meta schema location '#/properties/description' (code 5182)
Got expected error: JSON validation schema location #/description failed requirement: 'type' at meta schema location '#/properties/description' (code 5182)

#
# I.6
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'type' at meta schema location '#/properties/multipleOf' (code 5182)
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'type' at meta schema location '#/properties/multipleOf' (code 5182)

#
# I.7
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'minimum' at meta schema location '#/properties/multipleOf' (code 5182)
Got expected error: JSON validation schema location #/multipleOf failed requirement: 'minimum' at meta schema location '#/properties/multipleOf' (code 5182)

#
# I.8
Got expected error: JSON validation schema location #/maximum failed requirement: 'type' at meta schema location '#/properties/maximum' (code 5182)
Got expected error: JSON validation schema location #/maximum failed requirement: 'type' at meta schema location '#/properties/maximum' (code 5182)

#
# I.9
Got expected error: JSON validation schema location #/exclusiveMaximum failed requirement: 'type' at meta schema location '#/properties/exclusiveMaximum' (code 5182)
Got expected error: JSON validation schema location #/exclusiveMaximum failed requirement: 'type' at meta schema location '#/properties/exclusiveMaximum' (code 5182)

#
# I.10
Got expected error: JSON validation schema location #/minimum failed requirement: 'type' at meta schema location '#/properties/minimum' (code 5182)
Got expected error: JSON validation schema location #/minimum failed requirement: 'type' at meta schema location '#/properties/minimum' (code 5182)

#
# I.11
Got expected error: JSON validation schema location #/exclusiveMinimum failed requirement: 'type' at meta schema location '#/properties/exclusiveMinimum' (code 5182)
Got expected error: JSON validation schema location #/exclusiveMinimum failed requirement: 'type' at meta schema location '#/properties/exclusiveMinimum' (code 5182)

#
# I.12
Got expected error: JSON validation schema location #/maxLength failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxLength failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.13
Got expected error: JSON validation schema location #/maxLength failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxLength failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.14
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.15
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minLength failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.16
Got expected error: JSON validation schema location #/pattern failed requirement: 'type' at meta schema location '#/properties/pattern' (code 5182)
Got expected error: JSON validation schema location #/pattern failed requirement: 'type' at meta schema location '#/properties/pattern' (code 5182)

#
# I.17
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)

#
# I.18
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)
Got expected error: JSON validation schema location #/additionalItems failed requirement: 'anyOf' at meta schema location '#/properties/additionalItems' (code 5182)

#
# I.19
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.20
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.21
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)
Got expected error: JSON validation schema location #/items failed requirement: 'anyOf' at meta schema location '#/properties/items' (code 5182)

#
# I.22
Got expected error: JSON validation schema location #/maxItems failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxItems failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.23
Got expected error: JSON validation schema location #/maxItems failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxItems failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.24
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.25
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minItems failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.26
Got expected error: JSON validation schema location #/uniqueItems failed requirement: 'type' at meta schema location '#/properties/uniqueItems' (code 5182)
Got expected error: JSON validation schema location #/uniqueItems failed requirement: 'type' at meta schema location '#/properties/uniqueItems' (code 5182)

#
# I.27
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'type' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.28
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)
Got expected error: JSON validation schema location #/maxProperties failed requirement: 'minimum' at meta schema location '#/definitions/positiveInteger' (code 5182)

#
# I.29
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.30
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)
Got expected error: JSON validation schema location #/minProperties failed requirement: 'allOf' at meta schema location '#/definitions/positiveIntegerDefault0' (code 5182)

#
# I.31
Got expected error: JSON validation schema location #/required failed requirement: 'type' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required failed requirement: 'type' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.32
Got expected error: JSON validation schema location #/required failed requirement: 'minItems' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required failed requirement: 'minItems' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.33
Got expected error: JSON validation schema location #/required/0 failed requirement: 'type' at meta schema location '#/definitions/stringArray/items' (code 5182)
Got expected error: JSON validation schema location #/required/0 failed requirement: 'type' at meta schema location '#/definitions/stringArray/items' (code 5182)

#
# I.34
Got expected error: JSON validation schema location #/required/1 failed requirement: 'uniqueItems' at meta schema location '#/definitions/stringArray' (code 5182)
Got expected error: JSON validation schema location #/required/1 failed requirement: 'uniqueItems' at meta schema location '#/definitions/stringArray' (code 5182)

#
# I.35
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)

#
# I.36
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/additionalProperties failed requirement: 'anyOf' at meta schema location '#/properties/additionalProperties' (code 5182)

#
# I.37
Got expected error: JSON validation schema location #/definitions failed requirement: 'type' at meta schema location '#/properties/definitions' (code 5182)
Got expected error: JSON validation schema location #/definitions failed requirement: 'type' at meta schema location '#/properties/definitions' (code 5182)

#
# I.38
Got expected error: JSON validation schema location #/definitions/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/definitions/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.39
Got expected error: JSON validation schema location #/properties failed requirement: 'type' at meta schema location '#/properties/properties' (code 5182)
Got expected error: JSON validation schema location #/properties failed requirement: 'type' at meta schema location '#/properties/properties' (code 5182)

#
# I.40
Got expected error: JSON validation schema location #/properties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/properties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.41
Got expected error: JSON validation schema location #/patternProperties failed requirement: 'type' at meta schema location '#/properties/patternProperties' (code 5182)
Got expected error: JSON validation schema location #/patternProperties failed requirement: 'type' at meta schema location '#/properties/patternProperties' (code 5182)

#
# I.42
Got expected error: JSON validation schema location #/patternProperties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/patternProperties/def/invalid failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.43
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.44
Got expected error: JSON validation schema location #/dependencies/unknown failed requirement: 'anyOf' at meta schema location '#/properties/dependencies/additionalProperties' (code 5182)
Got expected error: JSON validation schema location #/dependencies/unknown failed requirement: 'anyOf' at meta schema location '#/properties/dependencies/additionalProperties' (code 5182)

#
# I.45
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.46
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.47
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)
Got expected error: JSON validation schema location #/dependencies failed requirement: 'type' at meta schema location '#/properties/dependencies' (code 5182)

#
# I.48
Got expected error: JSON validation schema location #/enum failed requirement: 'type' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum failed requirement: 'type' at meta schema location '#/properties/enum' (code 5182)

#
# I.49
Got expected error: JSON validation schema location #/enum failed requirement: 'minItems' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum failed requirement: 'minItems' at meta schema location '#/properties/enum' (code 5182)

#
# I.50
Got expected error: JSON validation schema location #/enum/1 failed requirement: 'uniqueItems' at meta schema location '#/properties/enum' (code 5182)
Got expected error: JSON validation schema location #/enum/1 failed requirement: 'uniqueItems' at meta schema location '#/properties/enum' (code 5182)

#
# I.51
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.52
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.53
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.54
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)
Got expected error: JSON validation schema location #/type failed requirement: 'anyOf' at meta schema location '#/properties/type' (code 5182)

#
# I.55
Got expected error: JSON validation schema location #/format failed requirement: 'type' at meta schema location '#/properties/format' (code 5182)
Got expected error: JSON validation schema location #/format failed requirement: 'type' at meta schema location '#/properties/format' (code 5182)

#
# I.56
Got expected error: JSON validation schema location #/allOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/allOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.57
Got expected error: JSON validation schema location #/allOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/allOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.58
Got expected error: JSON validation schema location #/allOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/allOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.59
Got expected error: JSON validation schema location #/anyOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/anyOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.60
Got expected error: JSON validation schema location #/anyOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/anyOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.61
Got expected error: JSON validation schema location #/anyOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/anyOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.62
Got expected error: JSON validation schema location #/oneOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/oneOf failed requirement: 'type' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.63
Got expected error: JSON validation schema location #/oneOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)
Got expected error: JSON validation schema location #/oneOf failed requirement: 'minItems' at meta schema location '#/definitions/schemaArray' (code 5182)

#
# I.64
Got expected error: JSON validation schema location #/oneOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/oneOf/0/unknown failed requirement: 'additionalProperties' at meta schema location '#' (code 5182)

#
# I.65
Got expected error: JSON validation schema location #/not failed requirement: 'type' at meta schema location '#' (code 5182)
Got expected error: JSON validation schema location #/not failed requirement: 'type' at meta schema location '#' (code 5182)
Mysqlx.Ok {
  msg: "bye!"
}
ok
