#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vec.h"

struct vec vec_new(size_t size) {
    struct vec vec;
    vec.capacity = 0;
    vec.len = 0;
    vec.size = size;
    vec.ptr = NULL;
    return vec;
}

int vec_push(struct vec *vec, void *val) {
    if (vec->len == vec->capacity) {
        if (vec->capacity == VEC_MAX) {
            errno = ENOMEM;
            return -1;
        }
        const size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity * 2;

        void *new_ptr = realloc(vec->ptr, new_capacity * vec->size);
        if (new_ptr == NULL) {
            return -1;
        }
        vec->ptr = new_ptr;
        vec->capacity = new_capacity;
    }
    memcpy(vec->ptr + vec->len * vec->size, val, vec->size);
    vec->len++;
    return 0;
}
