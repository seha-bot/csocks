#ifndef VEC
#define VEC

#include <stddef.h>
#include <stdint.h>

#define VEC_MAX (SIZE_MAX >> 1)

struct vec {
    size_t capacity;
    size_t len;
    size_t size;
    void *ptr;
};

// Create a new vec instance with 0 capacity, 0 len, NULL ptr
// and set the size for each element.
struct vec vec_new(size_t size);

// Push a value into vec. Vec's capacity will increase if len reaches it.
// Capacity can only double itself. Promotions can potentially ruin your day,
// so make sure that the value val points to is the correct type and has the
// same size as vec->size. After the first call to this function, you take
// responsibility of vec->ptr and it should be ultimately free'd.
// RETURN:
//      Success -> 0
//      If realloc fails, or capacity reaches VEC_MAX -> -1
int vec_push(struct vec *vec, void *val);

#endif /* VEC */
