# Copyright 2026 Google LLC

generate_floats() {
# Generate vectors with i ... i+16K values
floats=$(awk -v count="$1" -v starting_val="$2" \
            'BEGIN{ \
                        for (i=starting_val; i<count+starting_val; ++i) \
                        printf("%20.7f, ", i)
                  }'
)
# Remove the trailing comma
floats="${floats::-2}"

# Surround with square brackets
formatted_floats="string_to_vector('[$floats]')"

echo "$formatted_floats"
}

generate_floats_random() {
# Generate random floats
floats=$(awk -v count="$1" -v seed="$RANDOM" \
            'BEGIN{ srand(seed); \
                        for (i=0; i<count; ++i) \
                        printf("%20.7f, ", rand())
                  }'
)
# Remove the trailing comma
floats="${floats::-2}"

# Surround with square brackets
formatted_floats="string_to_vector('[$floats]')"

echo "$formatted_floats"
}

main() {
      table_name=$1
      dimensions=$2
      rows=$3
      deterministic=$4
      bulk=${5:-0}
      add_one_less_dimension=${6:-0}

      echo "CREATE TABLE $table_name(i INT PRIMARY KEY AUTO_INCREMENT, j VECTOR($dimensions));"

      if [ $bulk -gt 0 ]; then
        echo "INSERT INTO $table_name (i, j) VALUES"
      fi

      for ((k=1; k<=$rows; k++))
      do
        current_dimensions=$dimensions
        # For testing, make the second to last row have one fewer dimension
        if [ $add_one_less_dimension -gt 0 ] && [ $k -eq $((rows - 1)) ] && [ $rows -gt 1 ]; then
            current_dimensions=$((dimensions - 1))
        fi

        if [ $deterministic -gt 0 ]; then
            my_floats=$(generate_floats $current_dimensions $k)
        else
            my_floats=$(generate_floats_random $current_dimensions)
        fi

        if [ $bulk -gt 0 ]; then
          if [ $k -lt $rows ]; then
              echo "($k, $my_floats),"
          else
              echo "($k, $my_floats);"
          fi
        else
          echo "INSERT INTO $table_name (j) VALUES ($my_floats);"
        fi
      done
}

# Call the main function
main "$1" "$2" "$3" "$4" "$5" "$6"
