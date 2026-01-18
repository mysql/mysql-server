// Copyright (c) 2025, Oracle and/or its affiliates.

#ifndef VECTOR_OPERATIONS_INCLUDED
#define VECTOR_OPERATIONS_INCLUDED

#include <cstdint>

namespace vector_operations {

double l2_distance(const float *v1, const float *v2, uint32_t dimensions);
double cosine_similarity(const float *v1, const float *v2, uint32_t dimensions);
double cosine_distance(const float *v1, const float *v2, uint32_t dimensions);
double dot_product(const float *v1, const float *v2, uint32_t dimensions);

}

#endif
