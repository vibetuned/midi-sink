// Internal deformation queue: the per-frame list of ping-pong passes to run
// (PROJECT_SPEC.md §4.1, §6). This layer is deliberately sokol-free — it only
// describes passes; renderer.cpp dispatches them (working rule 2).
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    // Step 2: the minimal read-current/write-next pass. Drop / tine / vortex
    // deformations are added here in later steps.
    SUMI_DEFORM_PASSTHROUGH = 0
} sumi_deform_type_t;

typedef struct {
    sumi_deform_type_t type;
} sumi_deform_t;

typedef struct sumi_deform_queue_t sumi_deform_queue_t;

sumi_deform_queue_t* sumi_deform_queue_create(uint32_t capacity);
void     sumi_deform_queue_destroy(sumi_deform_queue_t* q);
// Returns false (and drops the pass) when the queue is full.
bool     sumi_deform_queue_push (sumi_deform_queue_t* q, const sumi_deform_t* deform);
uint32_t sumi_deform_queue_count(const sumi_deform_queue_t* q);
const sumi_deform_t* sumi_deform_queue_at(const sumi_deform_queue_t* q, uint32_t index);
void     sumi_deform_queue_clear(sumi_deform_queue_t* q);

#ifdef __cplusplus
}
#endif
