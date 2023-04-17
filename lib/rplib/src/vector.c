//
// Created by bktiel on 9/7/22.
//
#include "vector.h"

#include <string.h>

#include "rp_common.h"

// helper functions
static int vector_init_buffer(vector_t *p_vector,
                              size_t data_size,
                              size_t initial_cap);

static int vector_double_buffer(vector_t *p_vector);

// object def
typedef struct vector {
    size_t size;
    size_t data_size;

    void (*destructor)(void *);

    size_t capacity;
    void **vector_data_array;
} vector_t;

// getters
size_t
vector_get_size(vector_t *p_vector) {
    return p_vector->size;
}

void *
vector_get_buffer(vector_t *p_vector) {
    return p_vector->vector_data_array;
}

// body

/**
 * Create vector data structure for arbitrary data of size @data_size with
 * passed\n initial capacity and an optional destructor. Creates heap memory
 * allocation for struct and p_buf that must be freed manually or with
 * vector_destroy.
 * @param data_size Size of the data type to be stored in this vector.
 * @param initial_cap Initial capacity of this vector.
 * @param destructor Functional pointer to optional destructor called on vector
 * contents when vector is cleared or destroyed.
 * @return Pointer to generated vector data structure in memory
 */
vector_t *
vector_create(size_t data_size, size_t initial_cap, void (*destructor)(void *)) {
    vector_t initial_vec = VECTOR_STATIC_INITIALIZER;
    vector_t *vecbuf = NULL;

    // if invalid data size, return null.
    if (data_size < 1) {
        return NULL;
    }
    // create vector allocation
    vecbuf = malloc(sizeof(vector_t));
    if (!vecbuf) {
        return NULL;
    }
    // copy initial values over
    memcpy(vecbuf, &initial_vec, sizeof(vector_t));

    // set values
    vecbuf->vector_data_array = NULL;
    vecbuf->destructor = destructor;
    vecbuf->data_size = data_size;
    vecbuf->capacity = initial_cap;
    vecbuf->size = 0;

    // init p_buf
    if (1 == vector_init_buffer(vecbuf, data_size, initial_cap)) {
        vector_destroy(&vecbuf);
        return NULL;
    }
    return vecbuf;
}

/**
 * Allocate and assign a p_buf to a vector sufficient for a specified number
 * of elements for the vector's data type. If the vector already has a p_buf,
 * this function does nothing. Allocates memory that must be freed with
 * vector_erase, vector_destroy, or manually.\n
 * WARNING: If passed a p_vector that already has a p_buf, an error will occur
 * @param p_vector Pointer to vector object
 * @param data_size Size of data type this vector will hold
 * @param initial_cap Initial capacity of vector
 * @return 0 on success, 1 on failure
 */
static int
vector_init_buffer(vector_t *p_vector, size_t data_size, size_t initial_cap) {
    void **vec_buffer = NULL;
    // cannot initialize invalid vector
    if (!p_vector) {
        return RPLIB_UNSUCCESS;
    }
    if (p_vector->vector_data_array) {
        return RPLIB_UNSUCCESS;
    }
    // create vector p_buf
    vec_buffer = malloc(data_size * initial_cap);
    if (!vec_buffer) {
        return RPLIB_UNSUCCESS;
    }
    // success
    p_vector->vector_data_array = vec_buffer;
    return RPLIB_SUCCESS;
}

/**
 * Given a vector object and a pointer to data, copy data to the vector
 * p_buf,\n resizing or initializing the vector p_buf as necessary.
 * @param p_vector Pointer to vector object
 * @param p_data Pointer to data of size the vector was initialized with.
 * (e.g. vector initialized with int, 4 bytes, data must be *int)
 * @return 0 on success, 1 on failure
 */
int
vector_push_back(vector_t *p_vector, void *p_data) {
    // copy to back
    return vector_insert(p_vector, p_vector->size, p_data);
}

/**
 * Given a pointer to a vector object, retrieve the latest (top) value of the
 * vector and return a generic pointer
 * If the vector is empty, sets errno
 * @param p_vector Pointer to vector object
 * @return Generic pointer to data of size/type the vector was initialized with;
 * NULL + errno on error
 */
void *
vector_pop_back(vector_t *p_vector) {
    void *valptr = NULL;
    if (!p_vector || !p_vector->vector_data_array || (1 > p_vector->size)) {
        errno = ENOENT;
        return NULL;
    }

    // get last value and decrement size
    valptr = vector_at(p_vector, --p_vector->size);
    return valptr;
}

/**
 * Insert data of vector type before a given position in the vector, resizing
 * or initializing vector p_buf as necessary. If a value is at the provided
 * position, all values in the vector will be shifted by 1 to accomodate the
 * new entry. If there is not a value, functions identically to vector_set.\n
 * Note: NULL is not acceptable value
 * @param p_vector Pointer to vector object
 * @param pos Index to insert data before
 * @param p_value Pointer to not-null data of size the vector was initialized
 * with. (e.g. vector initialized with int, 4 bytes, data must be *int)
 * @return 0 on success, 1 on failure
 */
int
vector_insert(vector_t *p_vector, size_t pos, void *p_value) {
    // check valid object
    if (!p_vector) {
        return RPLIB_UNSUCCESS;
    }
    // check valid pos
    if (pos > p_vector->size) {
        return RPLIB_UNSUCCESS;
    }
    // if no p_buf, initialize
    if (!p_vector->vector_data_array) {
        if (RPLIB_UNSUCCESS == vector_init_buffer(p_vector, p_vector->data_size, 1)) {
            return RPLIB_UNSUCCESS;
        }
    }
    // check valid data
    if (!p_value) {
        return RPLIB_UNSUCCESS;
    }
    // if adding element would make full, attempt resize
    if ((p_vector->size + 1) == p_vector->capacity) {
        if (0 != vector_double_buffer(p_vector)) {
            return RPLIB_UNSUCCESS;
        }
    }
    // relocate existing values up by 1 if there is already a value
    if (NULL != vector_at(p_vector, pos)) {
        for (size_t migratePos = p_vector->size; migratePos > pos; migratePos--) {
            memcpy(vector_at(p_vector, migratePos),
                   vector_at(p_vector, migratePos - 1),
                   p_vector->data_size);
        }
    }
    // copy to specified index
    // zero out since memory is re-used
    memset(vector_at(p_vector, pos), 0, p_vector->data_size);
    memcpy(vector_at(p_vector, pos), p_value, p_vector->data_size);
    // set fields
    p_vector->size++;

    return RPLIB_SUCCESS;
}

/**
 * Remove item at a given index from the vector
 * @param p_vector object to evaluate
 * @param pos position to remove value from

 * @return 0 on success, 1 on error
 */
int
vector_remove(vector_t *p_vector, size_t pos) {
    size_t i = 0;
    u_int8_t *remove_addr = NULL;
    // check valid object
    if (!p_vector) {
        return RPLIB_UNSUCCESS;
    }
    // check valid pos
    if (0 == p_vector->size || pos >= p_vector->size) {
        return RPLIB_UNSUCCESS;
    }
    // call destructor as applicable
    remove_addr = (u_int8_t *) vector_at(p_vector, pos);
    if (NULL != p_vector->destructor) {
        p_vector->destructor(remove_addr);
    }

        if ((pos + 1) < p_vector->size) {
            // move all elements behind pos to the forward by 1
            memcpy(remove_addr,
                   remove_addr + p_vector->data_size,
                   ((p_vector->size - 1) - pos) * p_vector->data_size);
        }
        // decrement values
        p_vector->size -= 1;

    return RPLIB_SUCCESS;
}

/**
 * Manually resize a given vector's p_buf to give the vector a new specified
 * capacity. If new_cap is not greater than current vector capacity, this
 * function does nothing.
 * @param p_vector Pointer to vector object
 * @param new_cap New vector capacity in number of elements
 * @return 0 on success, 1 on error
 */
int
vector_reserve(vector_t *p_vector, size_t new_cap) {
    void **new_buf = NULL;
    // cannot action invalid vector
    if (!p_vector) {
        return RPLIB_UNSUCCESS;
    }
    // no change if below current capacity
    if (new_cap < p_vector->capacity) {
        return RPLIB_SUCCESS;
    }
    // create allocation
    new_buf = malloc(new_cap * p_vector->data_size);
    if (!new_buf) {
        return RPLIB_UNSUCCESS;
    }
    // copy then free old allocation
    if (NULL != p_vector->vector_data_array) {
        memcpy(new_buf,
               p_vector->vector_data_array,
               (p_vector->data_size * p_vector->size));
        free(p_vector->vector_data_array);
        p_vector->vector_data_array = NULL;
    }
    // assign
    p_vector->vector_data_array = new_buf;
    p_vector->capacity = new_cap;
    return RPLIB_SUCCESS;
}

/**
 * Utility function that doubles the size of a vector's underlying heap p_buf
 * @param p_vector Pointer to vector object to operate on.
 * @return 0 on success, 1 on error
 */
static int
vector_double_buffer(vector_t *p_vector) {
    size_t new_cap = 2;
    void **new_buf = NULL;
    // cannot work with invalid vector
    if (!p_vector) {
        return RPLIB_UNSUCCESS;
    }
    // if size 0, default to 2. Otherwise, create doubled allocation
    new_cap = (p_vector->capacity == 0) ? 2 : (p_vector->capacity * 2);
    return vector_reserve(p_vector, new_cap);
}

/**
 * Clear all entries in a given vector object by calling the vector's type
 * destructor and freeing the vector p_buf.
 * @param p_vector Pointer to vector object
 * @return 0 on success, 1 on error/failure
 */
int
vector_clear(vector_t *p_vector) {
    if (!p_vector) {
        return RPLIB_UNSUCCESS;
    }
    // if no p_buf, in expected state
    if (!p_vector->vector_data_array) {
        return RPLIB_SUCCESS;
    }
    // call destructor on elements if defined
    if (p_vector->destructor != NULL) {
        for (size_t i = 0; i < p_vector->size; i++) {
            p_vector->destructor(vector_at(p_vector, i));
        }
    }
    // set fields
    p_vector->size = 0;
    p_vector->capacity = 0;
    // free allocation
    free(p_vector->vector_data_array);
    p_vector->vector_data_array = NULL;

    return RPLIB_SUCCESS;
}

/**
 * Destroys a vector object by freeing the vector p_buf and the vector object
 * in memory. Calls the vector type destructor on all elements in the p_buf.
 * @param p_vector Pointer to vector object
 * @return 0 on success, 1 on error
 */
int
vector_destroy(vector_t **p_vector) {
    if (!p_vector) {
        return RPLIB_UNSUCCESS;
    }
    // erase p_buf
    if (vector_clear(*p_vector)) {
        return RPLIB_UNSUCCESS;
    }
    // destroy object
    free(*p_vector);
    *p_vector = NULL;

    return RPLIB_SUCCESS;
}

/**
 * Utility function to change a specific index in the given vector.\n
 * Note: Null is not valid data
 * @param p_vector Pointer to given vector object
 * @param pos Vector index to set value
 * @param data Pointer to data to write to @pos
 * @return 0 on success, 1 on failure
 */
int
vector_set(vector_t *p_vector, size_t pos, void *p_value) {
    void *entry = NULL;
    // validate
    if (!p_vector || !p_vector->vector_data_array
        || pos > (p_vector->capacity - 1)) {
        errno = ENOENT;
        return RPLIB_UNSUCCESS;
    }
    if (!p_value) {
        return RPLIB_UNSUCCESS;
    }
    // set value
    entry = vector_at(p_vector, pos);
    if (entry) {
        // copy over
        memcpy(entry, p_value, p_vector->data_size);
        return RPLIB_SUCCESS;
    }
    // bad
    return RPLIB_UNSUCCESS;
}

/**
 * Utility function to retrieve specific entry from a given vector.\n
 * Sets errno if entry not found
 * @param p_vector Pointer to given vector object
 * @param pos Index to retrieve from vector
 * @return Pointer to value at index @pos, NULL & errno on failure
 */
void *
vector_at(vector_t *p_vector, size_t pos) {
    void *entry = NULL;
    if (!p_vector || !p_vector->vector_data_array
        || pos > (p_vector->capacity - 1)) {
        errno = ENOENT;
        return NULL;
    }
    // use cast to char for single byte control
    entry = (char *) (p_vector->vector_data_array) + (p_vector->data_size * pos);
    return entry;
}
