//
// Created by bktiel on 9/7/22.
//

#ifndef JQRCALCPROJECTS_VECTOR_H
#define JQRCALCPROJECTS_VECTOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <errno.h>
#include <stdlib.h>

#define CREATE_VECTOR(data_type,initial_cap, destructor) ( \
        vector_create(sizeof(data_type),initial_cap,destructor)) \


// static init

#define VECTOR_STATIC_INITIALIZER                                     \
    {                                                                 \
        .size = 0, .data_size = 0, .capacity = 0, .destructor = NULL, \
        .vector_data_array = NULL                                     \
    }

    typedef struct vector vector_t;
vector_t *vector_create(size_t data_size, size_t initial_cap, void (*destructor)(void *));

int vector_push_back(vector_t *p_vector, void *data);
void* vector_pop_back(vector_t *p_vector);
void *vector_at(vector_t *p_vector, size_t pos);
int vector_insert(vector_t *p_vector, size_t pos, void *p_value);
int vector_remove(vector_t *p_vector, size_t pos);

int vector_reserve(vector_t *p_vector, size_t new_cap);

int vector_clear(vector_t *p_vector);
int vector_destroy(vector_t **p_vector);

// getters
void *vector_get_buffer(vector_t *p_vector);
size_t vector_get_size(vector_t *p_vector);


#ifdef __cplusplus
}
#endif

#endif // JQRCALCPROJECTS_VECTOR_H
