// displacement.cpp — deformation queue -> shader pass dispatch descriptions
// (PROJECT_SPEC.md §6). Fixed-capacity array, no allocation after create.
#include "displacement.h"

#include <stdlib.h>

struct sumi_deform_queue_t {
    sumi_deform_t* items;
    uint32_t       capacity;
    uint32_t       count;
};

extern "C" {

sumi_deform_queue_t* sumi_deform_queue_create(uint32_t capacity) {
    if (capacity == 0) return nullptr;
    sumi_deform_queue_t* q = (sumi_deform_queue_t*)calloc(1, sizeof(sumi_deform_queue_t));
    if (!q) return nullptr;
    q->items = (sumi_deform_t*)calloc(capacity, sizeof(sumi_deform_t));
    if (!q->items) {
        free(q);
        return nullptr;
    }
    q->capacity = capacity;
    return q;
}

void sumi_deform_queue_destroy(sumi_deform_queue_t* q) {
    if (!q) return;
    free(q->items);
    free(q);
}

bool sumi_deform_queue_push(sumi_deform_queue_t* q, const sumi_deform_t* deform) {
    if (!q || !deform || q->count >= q->capacity) return false;
    q->items[q->count++] = *deform;
    return true;
}

uint32_t sumi_deform_queue_count(const sumi_deform_queue_t* q) {
    return q ? q->count : 0;
}

const sumi_deform_t* sumi_deform_queue_at(const sumi_deform_queue_t* q, uint32_t index) {
    if (!q || index >= q->count) return nullptr;
    return &q->items[index];
}

void sumi_deform_queue_clear(sumi_deform_queue_t* q) {
    if (q) q->count = 0;
}

} // extern "C"
