#include "../shm/shm_ring.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  struct shm_ring_ctrl r;
  memset(&r, 0, sizeof(r));

  int rc = shm_ring_init(&r, 8, 64, 0);
  assert(rc == 0);

  // Push up to capacity.
  for (int i = 0; i < 8; i++) {
    uint32_t prod = 0;
    assert(shm_ring_push_begin(&r, &prod));
    shm_ring_push_commit(&r, prod);
  }
  {
    uint32_t prod = 0;
    assert(!shm_ring_push_begin(&r, &prod));
  }

  // Pop all.
  for (int i = 0; i < 8; i++) {
    uint32_t cons = 0;
    assert(shm_ring_pop_begin(&r, &cons));
    shm_ring_pop_commit(&r, cons);
  }
  {
    uint32_t cons = 0;
    assert(!shm_ring_pop_begin(&r, &cons));
  }

  // Wrap-around behavior under ping-pong.
  for (int i = 0; i < 1000; i++) {
    uint32_t prod = 0, cons = 0;
    assert(shm_ring_push_begin(&r, &prod));
    shm_ring_push_commit(&r, prod);
    assert(shm_ring_pop_begin(&r, &cons));
    shm_ring_pop_commit(&r, cons);
  }

  printf("OK\n");
  return 0;
}
