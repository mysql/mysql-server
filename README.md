# Vector Search for MySQL

> **DISCLAIMER & NOTICE**  
> Copyright 2026 Google LLC  
>  
> This project is an **experimental, draft proof-of-concept** released for learning, research, and exploration purposes only. It is **not** an officially supported Google product, and it is **not intended for production use**.

Authors: Inaam Rana, Shu Zhou, Venkatesh Duggirala, Julia Offerman, Ilavaluthy Mahendran, Xukai Xin, Karthic Kumar Sekar & Mowhebat Bazargani.

## Overview

This repository integrates a KMeans vector indexing library into MySQL InnoDB. It provides:

- Vector indexing
-  Approximate Nearest Neighbor (ANN) search directly within SQL queries via `APPROX_DISTANCE()`.

Refer to [this github issue](https://github.com/orgs/mysql/projects/2/views/1?pane=issue&itemId=195541905&issue=mysql%7Cmysql-community%7C3) for detailed LLD documents of the code and syntax.

## 1. Build `libscann.so`

Before building MySQL, you must compile `libscann.so`. A self-contained script is provided in `kmeans_interface/build_scann.sh`.

```bash
# 1. Navigate to the interface directory
cd kmeans_interface

# 2. Run the build script
./build_scann.sh
```
## 2. Build MySQL

Follow standard MySQL build instructions in `Docs/README.build`.

## 3. Start MySQL with runtime library path and vector enabled

Ensure `libscann.so` is in your runtime dynamic linker path when starting `mysqld`:

```bash
export LD_LIBRARY_PATH=/path/to/mysql/kmeans_interface:$LD_LIBRARY_PATH
./bin/mysqld --cloudsql_vector=ON...
```

## 4. SQL Syntax & Usage Guide

### A. Creating a Table with a Vector Column

Vector columns are declared with `VECTOR(dimensions)`:

```sql
CREATE TABLE items (
    id INT PRIMARY KEY AUTO_INCREMENT,
    title VARCHAR(255),
    embedding VECTOR(128)
);
```

### B. Creating a Vector Index

Vector indexes are created with `CREATE VECTOR INDEX`.

```sql
-- Create an L2-squared index
CREATE VECTOR INDEX idx_items_embedding ON items(embedding)
    DISTANCE_MEASURE=L2_SQUARED;

-- Or create a Cosine distance index
CREATE VECTOR INDEX idx_items_embedding ON items(embedding) [USING TREE] [QUANTIZER=SQ8] DISTANCE_MEASURE=l2_squared [NUM_LEAVES=2000]
```


### C. Querying (Approximate Nearest Neighbors)

Use `APPROX_DISTANCE()` in the `ORDER BY` clause with a `LIMIT` to perform an indexed ANN search:

```sql
-- Find the 10 closest items to a query vector
SELECT id, title,
       APPROX_DISTANCE(embedding, STRING_TO_VECTOR('[0.10, -0.40, ... 128 floats ...]'), 'distance_measure=cosine') AS dist
FROM items
ORDER BY dist
LIMIT 10;
```

### D. Dropping an Index

```sql
DROP INDEX idx_items_embedding ON items;
```
