# ==== Purpose ====
#
# This is intended both a functional test and a usage example for the
# framework to process JSON specifications in mtr. It currently covers
# three scripts:
#
#   include/create_json_iterator.inc
#   include/create_json_unpacker.inc
#   include/create_json_unpacking_iterator.inc
#
# See comments in those files for the full usage of each files.

--echo ==== Minimal example of JSON iterator ====

# Construct the iterator
--let $json_label = planet
--source include/create_json_iterator.inc

# Reset the iterator to the first element of this array.
--let $json_array = ["Mercury", "Venus", "Earth", "Mars" ]
--source $json_planet_start
while (!$json_planet_done) {
  # Do something useful with the current value.
  --echo Planet: $json_planet_value
  # Step forward to the next element.
  --source $json_planet_next
}
--source include/destroy_json_functions.inc

--echo ==== Example of nested JSON iterator ====

# Create iterator of outer loop.
--let $json_label = continent
--source include/create_json_iterator.inc
# Create iterator of inner loop.
--let $json_label = country
--source include/create_json_iterator.inc

# Reset the outer iterator to the first element of the outer array.
let $json_array = [
  ["Afghanistan", "Bangladesh", "Cambodia"],
  ["Albania", "Belgium", "Cyprus"],
  ["Argentina", "Bolivia", "Colombia"],
  ["Angola", "Burundi", "Cameroon"]
];
--source $json_continent_start
while (!$json_continent_done) {
  # Print the current inner array.
  --echo These countries belong to the same continent: $json_continent_value
  # Reset the inner iterator to the first element of the current inner array.
  --let $json_array = $json_continent_value
  --source $json_country_start
  while (!$json_country_done) {
    # Do something useful with the current value.
    --echo Country: $json_country_value
    # Step forward to the next element of the inner array.
    --source $json_country_next
  }
  # Step forward to the next element of the outer array.
  --source $json_continent_next
}
--source include/destroy_json_functions.inc

--echo ==== Minimal example of JSON unpacker ====

# Create the unpacker.
--let $json_label = country
--let $json_keys = name, capital
--source include/create_json_unpacker.inc

# Use the unpacker to unpack values from a JSON object.
--let $json_object = { "name": "Madagascar", "capital": "Antananarivo" }
--source $json_country_unpack

# Do something useful with the unpacked values.
--echo $capital is the capital of $name.

# Use the same unpacker to unpack values from another JSON object.
--let $json_object = { "name": "Iceland", "capital": "Reykjavik" }
--source $json_country_unpack

# Do something useful with the unpacked values.
--echo $capital is the capital of $name.

# Clean up the generated files
--source include/destroy_json_functions.inc

--echo ==== Minimal example of unpacking JSON iterator ====

# Create the unpacking iterator.
--let $json_label = book
--let $json_keys = author, title
--source include/create_json_unpacking_iterator.inc

# Reset the iterator to the first element of this array.
let $json_array = [
  {
    "author": "Kerstin Ekman",
    "title": "Grand final i skojarbranschen"
  },
  {
    "author": "Bjarne Stroustrup",
    "title": "The C++ Programming Language"
  },
  {
    "author": "Pelle Holmberg and Hans Marklund",
    "title": "Nya svampboken"
  }
];
--source $json_book_start
while (!$json_book_done) {
  # Do something useful with the unpacked values.
  --echo $author wrote $title
  # Step forward to the next element.
  --source $json_book_next
}

--echo ==== Example of missing keys to unpack ====

# Create an unpacking iterator
--let $json_label = plant
--let $json_keys = name, fruit_color, flower_color
--source include/create_json_unpacking_iterator.inc

# Reset the iterator to the first element of this array.
let $json_array = [
  {
    "name": "Apple",
    "fruit_color": "red",
    "flower_color": "white"
  },
  {
    "name": "Tussilago",
    "flower_color": "yellow"
  },
  {
    "name": "Chanterelle",
    "fruit_color": "yellow"
  }
];
--source $json_plant_start
while (!$json_plant_done) {
  # Do something useful with the unpacked values.
  # Non-existing values are empty.
  if ($fruit_color) {
    --echo $name has $fruit_color fruits.
  }
  if (!$fruit_color) {
    --echo $name has no fruits.
  }
  if ($flower_color) {
    --echo $name has $flower_color flowers.
  }
  if (!$flower_color) {
    --echo $name has no flowers.
  }
  # Step forward to the next plant.
  --source $json_plant_next
}

--echo ==== Example of default values and required keys ====

# Create iterator.
--let $json_label = animal
--let $json_keys = species, legs, color
# Require that every object has a "species" key.
--let $json_required = species
# For objects that don't have a "legs" key, assume legs=4.
--let $json_defaults = {"legs": 4}
--source include/create_json_unpacking_iterator.inc
--let $json_required =
--let $json_defaults =

# Reset the iterator to the first element of this array.
let $json_array = [
  { "species": "ant", "color": "brown", "legs": 6 },
  { "species": "bear", "color": "black" },
  { "species": "cat", "color": "orange" }
];
--source $json_animal_start
while (!$json_animal_done) {
  # Do something useful with the unpacked values.
  --echo The $species has $legs legs and is $color
  # Step forward to the next animal.
  --source $json_animal_next
}

--echo ==== Example of quoting and verbosity ====

# Create the unpacker.
--let $json_label = songs
--let $json_keys = first, second, third, fourth, fifth
--let $json_output_json_quoted = first, second
--let $json_output_single_quoted = first, third
--let $json_verbose = first, second, third, fourth
--source include/create_json_unpacker.inc

let $json_object = {
  "first": "Ain't Talkin'",
  "second": "Don't Think Twice, It's All Right",
  "third": "It Ain't Me, Babe",
  "fourth": "Rollin' and Tumblin'",
  "fifth": "Where are you?"
};
--source $json_songs_unpack

--echo ==== Example of reverse iteration, long steps, and forced position ====

# Create iterator.
--let $json_label = vegetable
--source include/create_json_iterator.inc

--echo # Reverse alphabetic vegetables
# Reset the iterator to the last element of this array and step backwards.
--let $json_array = ["artichoke", "broccoli", "cauliflower", "durian"]
--let $json_stride = -1
--source $json_vegetable_start
while (!$json_vegetable_done) {
  --echo Vegetable: $json_vegetable_value
  --source $json_vegetable_next
}

--echo # Alphabetic vegetables, even-numbered initial letter
# Reset the iterator to the first element of this array and jump two steps at a time.
--let $json_stride = 2
--source $json_vegetable_start
while (!$json_vegetable_done) {
  --echo Vegetable: $json_vegetable_value
  --source $json_vegetable_next
}

--echo # Alphabetic vegetables, odd-numbered initial letter
# Reset the iterator to the first element of this array, jump two steps at a time.
--let $json_stride = 2
--source $json_vegetable_start
# Jump to the element with index 1 (i.e., broccoli)
--let $json_vegetable_index = 1
--source $json_vegetable_next
while (!$json_vegetable_done) {
  --echo Vegetable: $json_vegetable_value
  --source $json_vegetable_next
}

--echo ==== Clean up ====

--source include/destroy_json_functions.inc
