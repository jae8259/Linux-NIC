#define _GNU_SOURCE

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef __linux__
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  fprintf(stderr, "SKIP: shm_bench requires Linux (memfd_create + eventfd).\n");
  return 0;
}
#else

#include "shm_ring.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>

static void die(const char *msg) {
  perror(msg);
  exit(1);
}

static bool streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

static uint64_t nsec_now(void) {
  struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
  const clockid_t clk = CLOCK_MONOTONIC_RAW;
#else
  const clockid_t clk = CLOCK_MONOTONIC;
#endif
  if (clock_gettime(clk, &ts) != 0)
    die("clock_gettime");
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int cmp_u64(const void *a, const void *b) {
  uint64_t ua = *(const uint64_t *)a;
  uint64_t ub = *(const uint64_t *)b;
  return (ua > ub) - (ua < ub);
}

static uint64_t pct(const uint64_t *sorted, size_t n, double p) {
  if (n == 0)
    return 0;
  double idx = p * (double)(n - 1);
  size_t i = (size_t)(idx + 0.5);
  if (i >= n)
    i = n - 1;
  return sorted[i];
}

static int memfd_create_compat(const char *name, unsigned int flags) {
#ifdef __linux__
  return (int)syscall(SYS_memfd_create, name, flags);
#else
  (void)name;
  (void)flags;
  errno = ENOSYS;
  return -1;
#endif
}

struct shm_layout {
  uint32_t magic;
  uint32_t version;
  uint32_t slot_stride;
  uint32_t max_msg_size;
  uint32_t capacity;
  uint32_t _pad0;
  struct shm_ring_ctrl ring_ab;
  struct shm_ring_ctrl ring_ba;
};

enum { SHM_MAGIC = 0x46505448u /* 'FPTH' */, SHM_VERSION = 1 };

struct shm_slot_hdr {
  uint32_t len;
  uint32_t seq;
  uint32_t checksum;
  uint32_t flags;
};

enum { SHM_FLAG_STOP = 1u };

static uint32_t fnv1a32(const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < len; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

static void *slot_ptr(void *base, size_t off, uint32_t idx, uint32_t stride) {
  return (uint8_t *)base + off + (size_t)idx * (size_t)stride;
}

static ssize_t send_fds(int sock, const void *buf, size_t len, const int *fds, size_t nfds) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));

  struct iovec iov;
  iov.iov_base = (void *)buf;
  iov.iov_len = len;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  uint8_t cmsg_buf[CMSG_SPACE(sizeof(int) * 8)];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int) * nfds);
  memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * nfds);

  msg.msg_controllen = CMSG_SPACE(sizeof(int) * nfds);
  return sendmsg(sock, &msg, 0);
}

static ssize_t recv_fds(int sock, void *buf, size_t len, int *fds, size_t nfds) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));

  struct iovec iov;
  iov.iov_base = buf;
  iov.iov_len = len;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  uint8_t cmsg_buf[CMSG_SPACE(sizeof(int) * 8)];
  memset(cmsg_buf, 0, sizeof(cmsg_buf));
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);

  ssize_t n = recvmsg(sock, &msg, 0);
  if (n < 0)
    return n;

  struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
  if (!cmsg || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
    return (errno = EPROTO), -1;
  size_t got = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
  if (got < nfds)
    return (errno = EMSGSIZE), -1;
  memcpy(fds, CMSG_DATA(cmsg), sizeof(int) * nfds);
  return n;
}

static int make_unix_server(const char *path) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    die("socket(AF_UNIX)");

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path) >= (int)sizeof(addr.sun_path)) {
    fprintf(stderr, "--sock path too long\n");
    exit(2);
  }

  unlink(path);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    die("bind(AF_UNIX)");
  if (listen(fd, 1) != 0)
    die("listen(AF_UNIX)");
  return fd;
}

static int make_unix_client(const char *path) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    die("socket(AF_UNIX)");

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path) >= (int)sizeof(addr.sun_path)) {
    fprintf(stderr, "--sock path too long\n");
    exit(2);
  }
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    die("connect(AF_UNIX)");
  return fd;
}

static void eventfd_kick(int efd) {
  uint64_t one = 1;
  ssize_t n = write(efd, &one, sizeof(one));
  if (n != (ssize_t)sizeof(one))
    die("eventfd write");
}

static void eventfd_wait(int efd) {
  uint64_t v = 0;
  ssize_t n = read(efd, &v, sizeof(v));
  if (n != (ssize_t)sizeof(v))
    die("eventfd read");
}

static void server_loop(void *shm, size_t shm_len, int efd_ab, int efd_ba) {
  (void)shm_len;
  struct shm_layout *l = (struct shm_layout *)shm;

  size_t rings_off = shm_align_up(sizeof(struct shm_layout), 64);
  size_t ring_ab_off = rings_off;
  size_t ring_ba_off = ring_ab_off + (size_t)l->capacity * (size_t)l->slot_stride;

  while (1) {
    uint32_t cons_before = 0;
    while (!shm_ring_pop_begin(&l->ring_ab, &cons_before))
      eventfd_wait(efd_ab);

    uint32_t idx = shm_ring_slot_index_for_pop(&l->ring_ab, cons_before);
    void *slot = slot_ptr(shm, ring_ab_off, idx, l->slot_stride);
    struct shm_slot_hdr hdr;
    memcpy(&hdr, slot, sizeof(hdr));

    if (hdr.flags & SHM_FLAG_STOP)
      break;
    if (hdr.len > l->max_msg_size) {
      fprintf(stderr, "server: invalid len=%u\n", hdr.len);
      break;
    }

    uint8_t *payload = (uint8_t *)slot + sizeof(struct shm_slot_hdr);
    uint32_t csum = fnv1a32(payload, hdr.len) ^ hdr.seq ^ hdr.len;
    if (csum != hdr.checksum) {
      fprintf(stderr, "server: checksum mismatch seq=%u\n", hdr.seq);
      break;
    }

    uint32_t prod_before = 0;
    while (!shm_ring_push_begin(&l->ring_ba, &prod_before))
      ; // ping-pong; should not happen unless client stalls

    uint32_t oidx = shm_ring_slot_index_for_push(&l->ring_ba, prod_before);
    void *oslot = slot_ptr(shm, ring_ba_off, oidx, l->slot_stride);

    struct shm_slot_hdr ohdr = hdr;
    memcpy(oslot, &ohdr, sizeof(ohdr));
    memcpy((uint8_t *)oslot + sizeof(struct shm_slot_hdr), payload, hdr.len);
    shm_ring_push_commit(&l->ring_ba, prod_before);
    eventfd_kick(efd_ba);

    shm_ring_pop_commit(&l->ring_ab, cons_before);
  }
}

static void usage(const char *prog) {
  fprintf(stderr,
          "Usage:\n"
          "  %s --mode server --sock /tmp/fastpath_shm.sock\n"
          "  %s --mode client --sock /tmp/fastpath_shm.sock --size 64 --iters 200000 [--out file]\n",
          prog, prog);
  exit(2);
}

int main(int argc, char **argv) {
  const char *mode = NULL;
  const char *sock_path = "/tmp/fastpath_shm.sock";
  uint32_t size = 64;
  uint64_t iters = 200000;
  const char *out_path = NULL;
  uint32_t capacity = 1024;
  uint32_t max_msg_size = 4096;

  for (int i = 1; i < argc; i++) {
    if (streq(argv[i], "--mode") && i + 1 < argc) {
      mode = argv[++i];
    } else if (streq(argv[i], "--sock") && i + 1 < argc) {
      sock_path = argv[++i];
    } else if (streq(argv[i], "--size") && i + 1 < argc) {
      size = (uint32_t)strtoul(argv[++i], NULL, 10);
    } else if (streq(argv[i], "--iters") && i + 1 < argc) {
      iters = strtoull(argv[++i], NULL, 10);
    } else if (streq(argv[i], "--out") && i + 1 < argc) {
      out_path = argv[++i];
    } else if (streq(argv[i], "--capacity") && i + 1 < argc) {
      capacity = (uint32_t)strtoul(argv[++i], NULL, 10);
    } else if (streq(argv[i], "--max-size") && i + 1 < argc) {
      max_msg_size = (uint32_t)strtoul(argv[++i], NULL, 10);
    } else if (streq(argv[i], "-h") || streq(argv[i], "--help")) {
      usage(argv[0]);
    } else {
      usage(argv[0]);
    }
  }

  if (!mode)
    usage(argv[0]);
  if (size > max_msg_size) {
    fprintf(stderr, "--size must be <= --max-size\n");
    return 2;
  }
  if (sizeof(struct shm_slot_hdr) + size > shm_align_up(sizeof(struct shm_slot_hdr) + max_msg_size, 64)) {
    fprintf(stderr, "internal: slot sizing error\n");
    return 2;
  }
  if (!shm_is_power_of_two(capacity)) {
    fprintf(stderr, "--capacity must be power-of-two\n");
    return 2;
  }

  uint32_t slot_stride = (uint32_t)shm_align_up(sizeof(struct shm_slot_hdr) + max_msg_size, 64);

  if (streq(mode, "server")) {
    int srv = make_unix_server(sock_path);
    int conn = accept(srv, NULL, NULL);
    if (conn < 0)
      die("accept");

    int memfd = memfd_create_compat("fastpath_shm", MFD_CLOEXEC);
    if (memfd < 0)
      die("memfd_create");

    int efd_ab = eventfd(0, EFD_CLOEXEC);
    if (efd_ab < 0)
      die("eventfd(ab)");
    int efd_ba = eventfd(0, EFD_CLOEXEC);
    if (efd_ba < 0)
      die("eventfd(ba)");

    size_t layout_off = shm_align_up(sizeof(struct shm_layout), 64);
    size_t shm_len = layout_off + 2ull * (size_t)capacity * (size_t)slot_stride;
    if (ftruncate(memfd, (off_t)shm_len) != 0)
      die("ftruncate(memfd)");

    void *shm = mmap(NULL, shm_len, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
    if (shm == MAP_FAILED)
      die("mmap(memfd)");
    memset(shm, 0, shm_len);

    struct shm_layout *l = (struct shm_layout *)shm;
    l->magic = SHM_MAGIC;
    l->version = SHM_VERSION;
    l->slot_stride = slot_stride;
    l->max_msg_size = max_msg_size;
    l->capacity = capacity;
    if (shm_ring_init(&l->ring_ab, capacity, slot_stride, 0) != 0)
      die("shm_ring_init(ab)");
    if (shm_ring_init(&l->ring_ba, capacity, slot_stride, 0) != 0)
      die("shm_ring_init(ba)");

    struct {
      uint32_t magic;
      uint32_t version;
      uint32_t shm_len;
      uint32_t _pad;
    } hello = {SHM_MAGIC, SHM_VERSION, (uint32_t)shm_len, 0};

    int fds[3] = {memfd, efd_ab, efd_ba};
    if (send_fds(conn, &hello, sizeof(hello), fds, 3) < 0)
      die("send_fds");

    server_loop(shm, shm_len, efd_ab, efd_ba);
    return 0;
  }

  if (streq(mode, "client")) {
    int conn = make_unix_client(sock_path);

    struct {
      uint32_t magic;
      uint32_t version;
      uint32_t shm_len;
      uint32_t _pad;
    } hello;
    int fds[3];
    ssize_t n = recv_fds(conn, &hello, sizeof(hello), fds, 3);
    if (n < 0)
      die("recv_fds");
    if (hello.magic != SHM_MAGIC || hello.version != SHM_VERSION) {
      fprintf(stderr, "bad hello from server\n");
      return 1;
    }

    int memfd = fds[0];
    int efd_ab = fds[1];
    int efd_ba = fds[2];
    size_t shm_len = hello.shm_len;

    void *shm = mmap(NULL, shm_len, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
    if (shm == MAP_FAILED)
      die("mmap(memfd)");
    struct shm_layout *l = (struct shm_layout *)shm;
    if (l->magic != SHM_MAGIC || l->version != SHM_VERSION) {
      fprintf(stderr, "bad shm header\n");
      return 1;
    }
    if (size > l->max_msg_size) {
      fprintf(stderr, "server max_msg_size too small\n");
      return 1;
    }

    size_t rings_off = shm_align_up(sizeof(struct shm_layout), 64);
    size_t ring_ab_off = rings_off;
    size_t ring_ba_off = ring_ab_off + (size_t)l->capacity * (size_t)l->slot_stride;

    uint8_t *payload = malloc(size);
    if (!payload)
      die("malloc(payload)");
    memset(payload, 0xAB, size);

    uint64_t *rtts = calloc((size_t)iters, sizeof(uint64_t));
    if (!rtts)
      die("calloc(rtts)");

    for (uint64_t i = 0; i < iters; i++) {
      uint32_t prod_before = 0;
      while (!shm_ring_push_begin(&l->ring_ab, &prod_before))
        ; // ping-pong

      uint32_t idx = shm_ring_slot_index_for_push(&l->ring_ab, prod_before);
      void *slot = slot_ptr(shm, ring_ab_off, idx, l->slot_stride);

      struct shm_slot_hdr hdr;
      hdr.len = size;
      hdr.seq = (uint32_t)i;
      hdr.flags = 0;
      hdr.checksum = fnv1a32(payload, size) ^ hdr.seq ^ hdr.len;

      memcpy(slot, &hdr, sizeof(hdr));
      memcpy((uint8_t *)slot + sizeof(hdr), payload, size);
      shm_ring_push_commit(&l->ring_ab, prod_before);

      uint64_t t0 = nsec_now();
      eventfd_kick(efd_ab);

      uint32_t cons_before = 0;
      while (!shm_ring_pop_begin(&l->ring_ba, &cons_before))
        eventfd_wait(efd_ba);

      uint32_t ridx = shm_ring_slot_index_for_pop(&l->ring_ba, cons_before);
      void *rslot = slot_ptr(shm, ring_ba_off, ridx, l->slot_stride);
      struct shm_slot_hdr rhdr;
      memcpy(&rhdr, rslot, sizeof(rhdr));
      uint8_t *rpayload = (uint8_t *)rslot + sizeof(rhdr);

      uint64_t t1 = nsec_now();
      rtts[i] = t1 - t0;

      if (rhdr.seq != (uint32_t)i || rhdr.len != size) {
        fprintf(stderr, "response mismatch seq=%u len=%u\n", rhdr.seq, rhdr.len);
        return 1;
      }
      uint32_t csum = fnv1a32(rpayload, size) ^ rhdr.seq ^ rhdr.len;
      if (csum != rhdr.checksum) {
        fprintf(stderr, "response checksum mismatch seq=%u\n", rhdr.seq);
        return 1;
      }

      shm_ring_pop_commit(&l->ring_ba, cons_before);
    }

    // Send stop
    uint32_t prod_before = 0;
    while (!shm_ring_push_begin(&l->ring_ab, &prod_before))
      ;
    uint32_t idx = shm_ring_slot_index_for_push(&l->ring_ab, prod_before);
    void *slot = slot_ptr(shm, ring_ab_off, idx, l->slot_stride);
    struct shm_slot_hdr stop = {.len = 0, .seq = 0, .checksum = 0, .flags = SHM_FLAG_STOP};
    memcpy(slot, &stop, sizeof(stop));
    shm_ring_push_commit(&l->ring_ab, prod_before);
    eventfd_kick(efd_ab);

    qsort(rtts, (size_t)iters, sizeof(uint64_t), cmp_u64);
    uint64_t p50 = pct(rtts, (size_t)iters, 0.50);
    uint64_t p90 = pct(rtts, (size_t)iters, 0.90);
    uint64_t p99 = pct(rtts, (size_t)iters, 0.99);
    uint64_t min = rtts[0];
    uint64_t max = rtts[(size_t)iters - 1];

    FILE *out = stdout;
    if (out_path) {
      out = fopen(out_path, "w");
      if (!out)
        die("fopen(--out)");
    }

    fprintf(out,
            "{"
            "\"path\":\"shm_memfd_eventfd\","
            "\"sock\":\"%s\","
            "\"size\":%u,"
            "\"iters\":%" PRIu64 ","
            "\"capacity\":%u,"
            "\"max_msg_size\":%u,"
            "\"rtt_ns\":{\"min\":%" PRIu64 ",\"p50\":%" PRIu64 ",\"p90\":%" PRIu64 ",\"p99\":%" PRIu64
            ",\"max\":%" PRIu64 "}"
            "}\n",
            sock_path, size, iters, l->capacity, l->max_msg_size, min, p50, p90, p99, max);

    if (out != stdout)
      fclose(out);
    return 0;
  }

  usage(argv[0]);
}

#endif
