// displacement.cpp — deformation queue -> shader pass dispatch descriptions
// (PROJECT_SPEC.md §6). Fixed-capacity array, no allocation after create.
#include "displacement.h"

#include <math.h>
#include <stdlib.h>

struct sumi_deform_queue_t {
    sumi_deform_t* items;
    uint32_t       capacity;
    uint32_t       count;
};

extern "C" {

// Crossed-tine pinch (v0.4 variant, DECISIONS_3 #34): the step-19 prototype
// verbatim — one tine along the fold axis, one along the perpendicular,
// through (x, y). Tines are infinite lines; the endpoints only fix direction.
void sumi_deform_crossed_pinch(float x, float y, float dir_x, float dir_y,
                               float k, sumi_deform_t out[2]) {
    const float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
    float ca = 1.0f, sa = 0.0f;
    if (len > 1e-9f) { ca = dir_x / len; sa = dir_y / len; }
    if (k < 0.0f) { ca = -ca; sa = -sa; }        // sign reverses both drags
    const float mag = (k >= 0.0f ? k : -k) * 0.2f;
    const float h = 0.1f;
    out[0].type = SUMI_DEFORM_TINE;
    out[0].as.tine.x0 = x - ca * h;  out[0].as.tine.y0 = y - sa * h;
    out[0].as.tine.x1 = x + ca * h;  out[0].as.tine.y1 = y + sa * h;
    out[0].as.tine.alpha = 0.03f;
    out[0].as.tine.magnitude = mag;
    out[1].type = SUMI_DEFORM_TINE;
    out[1].as.tine.x0 = x + sa * h;  out[1].as.tine.y0 = y - ca * h;
    out[1].as.tine.x1 = x - sa * h;  out[1].as.tine.y1 = y + ca * h;
    out[1].as.tine.alpha = 0.03f;
    out[1].as.tine.magnitude = mag;
}

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
