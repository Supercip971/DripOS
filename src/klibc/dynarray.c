#include "dynarray.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"

#include "drivers/serial.h"

int dynarray_remove(dynarray_t *dynarray, int64_t element) {
    int ret;
    lock(dynarray->lock);
    if (element > dynarray->array_size) {
        ret = -2;
        goto out;
    } else if (!dynarray->base[element].present) {
        ret = -1;
        goto out;
    }
    ret = 0;
    dynarray->base[element].present = 0;
    if (!atomic_dec(&dynarray->base[element].ref_count)) {
        kfree(dynarray->base[element].data);
        dynarray->base[element].data = (void *) 0;
    }
out:
    unlock(dynarray->lock);
    return ret;
}

void dynarray_unref(dynarray_t *dynarray, int64_t element) {
    lock(dynarray->lock);
    if (dynarray->base[element].data && !atomic_dec(&dynarray->base[element].ref_count)) {
        kfree(dynarray->base[element].data);
        dynarray->base[element].data = 0;
    }
    unlock(dynarray->lock);
}

void *dynarray_getelem(dynarray_t *dynarray, int64_t element) {
    lock(dynarray->lock);
    void *ptr = NULL;
    if (dynarray->array_size > element && dynarray->base[element].data && dynarray->base[element].present) {
        ptr = dynarray->base[element].data;
        atomic_inc(&dynarray->base[element].ref_count);
    }
    unlock(dynarray->lock);

    return ptr;
}

int64_t dynarray_add(dynarray_t *dynarray, void *element, uint64_t size_of_elem) {
    int64_t ret = -1;
    lock(dynarray->lock);

    int64_t i;
    for (i = 0; i < dynarray->array_size; i++) {
        if (!dynarray->base[i].data)
            goto fnd;
    }

    dynarray->array_size += 256;
    void *tmp = krealloc(dynarray->base, dynarray->array_size * sizeof(dynarray_elem_t));
    if (!tmp)
        goto out;
    dynarray->base = tmp;
fnd:
    dynarray->base[i].data = kcalloc(size_of_elem);
    if (!dynarray->base[i].data)
        goto out;
    memcpy((uint8_t *) element, (uint8_t *) dynarray->base[i].data, size_of_elem);
    dynarray->base[i].ref_count = 1;
    dynarray->base[i].present = 1;
    ret = i;
out:
    unlock(dynarray->lock);
    return ret;
}