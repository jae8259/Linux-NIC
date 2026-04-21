#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline size_t shm_align_up(size_t x, size_t a) { return (x + (a - 1)) & ~(a - 1); }

struct shm_ring_ctrl {
  uint32_t capacity;
  uint32_t slot_stride;
  uint32_t slot_data_off;
  uint32_t _reserved;

  _Alignas(64) uint32_t prod;
  uint8_t _pad0[60];
  _Alignas(64) uint32_t cons;
  uint8_t _pad1[60];
};

static inline uint32_t shm_ring_mask(const struct shm_ring_ctrl *r) { return r->capacity - 1u; }

static inline bool shm_is_power_of_two(uint32_t x) { return x && ((x & (x - 1u)) == 0); }

static inline int shm_ring_init(struct shm_ring_ctrl *r, uint32_t capacity, uint32_t slot_stride,
                                uint32_t slot_data_off) {
  if (!shm_is_power_of_two(capacity))
    return EINVAL;
  if (slot_stride == 0)
    return EINVAL;
  r->capacity = capacity;
  r->slot_stride = slot_stride;
  r->slot_data_off = slot_data_off;
  __atomic_store_n(&r->prod, 0u, __ATOMIC_RELAXED);
  __atomic_store_n(&r->cons, 0u, __ATOMIC_RELAXED);
  return 0;
}

static inline bool shm_ring_push_begin(const struct shm_ring_ctrl *r, uint32_t *prod_out) {
  uint32_t prod = __atomic_load_n(&r->prod, __ATOMIC_RELAXED);
  uint32_t cons = __atomic_load_n(&r->cons, __ATOMIC_ACQUIRE);
  if ((prod - cons) >= r->capacity)
    return false;
  *prod_out = prod;
  return true;
}

static inline void shm_ring_push_commit(struct shm_ring_ctrl *r, uint32_t prod_before) {
  __atomic_store_n(&r->prod, prod_before + 1u, __ATOMIC_RELEASE);
}

static inline bool shm_ring_pop_begin(const struct shm_ring_ctrl *r, uint32_t *cons_out) {
  uint32_t cons = __atomic_load_n(&r->cons, __ATOMIC_RELAXED);
  uint32_t prod = __atomic_load_n(&r->prod, __ATOMIC_ACQUIRE);
  if (cons == prod)
    return false;
  *cons_out = cons;
  return true;
}

static inline void shm_ring_pop_commit(struct shm_ring_ctrl *r, uint32_t cons_before) {
  __atomic_store_n(&r->cons, cons_before + 1u, __ATOMIC_RELEASE);
}

static inline uint32_t shm_ring_slot_index_for_push(const struct shm_ring_ctrl *r, uint32_t prod_before_inc) {
  return prod_before_inc & shm_ring_mask(r);
}

static inline uint32_t shm_ring_slot_index_for_pop(const struct shm_ring_ctrl *r, uint32_t cons_before_inc) {
  return cons_before_inc & shm_ring_mask(r);
}
