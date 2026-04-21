#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static void die(const char *msg) {
  perror(msg);
  exit(1);
}

static void usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s --peer <ip> [--port 7777] [--size 64] [--iters 100000] [--out file]\n"
          "  UDP RTT benchmark (ping-pong) against sock_echo.\n",
          prog);
  exit(2);
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

int main(int argc, char **argv) {
  const char *peer_ip = NULL;
  int port = 7777;
  size_t size = 64;
  uint64_t iters = 100000;
  int timeout_ms = 2000;
  const char *out_path = NULL;

  for (int i = 1; i < argc; i++) {
    if (streq(argv[i], "--peer") && i + 1 < argc) {
      peer_ip = argv[++i];
    } else if (streq(argv[i], "--port") && i + 1 < argc) {
      port = atoi(argv[++i]);
    } else if (streq(argv[i], "--size") && i + 1 < argc) {
      size = (size_t)strtoul(argv[++i], NULL, 10);
    } else if (streq(argv[i], "--iters") && i + 1 < argc) {
      iters = strtoull(argv[++i], NULL, 10);
    } else if (streq(argv[i], "--timeout-ms") && i + 1 < argc) {
      timeout_ms = atoi(argv[++i]);
    } else if (streq(argv[i], "--out") && i + 1 < argc) {
      out_path = argv[++i];
    } else if (streq(argv[i], "-h") || streq(argv[i], "--help")) {
      usage(argv[0]);
    } else {
      usage(argv[0]);
    }
  }

  if (!peer_ip)
    usage(argv[0]);
  if (size < sizeof(uint64_t)) {
    fprintf(stderr, "--size must be >= 8\n");
    return 2;
  }
  if (size > 65507) {
    fprintf(stderr, "--size too large for UDP payload\n");
    return 2;
  }

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    die("socket");
  if (timeout_ms > 0) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
      die("setsockopt(SO_RCVTIMEO)");
  }

  struct sockaddr_in peer;
  memset(&peer, 0, sizeof(peer));
  peer.sin_family = AF_INET;
  peer.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, peer_ip, &peer.sin_addr) != 1) {
    fprintf(stderr, "invalid --peer ip: %s\n", peer_ip);
    return 2;
  }

  uint8_t *buf = calloc(1, size);
  if (!buf)
    die("calloc");
  uint64_t *rtts = calloc((size_t)iters, sizeof(uint64_t));
  if (!rtts)
    die("calloc(rtts)");

  for (uint64_t i = 0; i < iters; i++) {
    memcpy(buf, &i, sizeof(i));
    uint64_t t0 = nsec_now();

    ssize_t sent = sendto(fd, buf, size, 0, (struct sockaddr *)&peer, sizeof(peer));
    if (sent < 0)
      die("sendto");
    if ((size_t)sent != size) {
      fprintf(stderr, "short send: %zd\n", sent);
      return 1;
    }

    ssize_t recvd = recvfrom(fd, buf, size, 0, NULL, NULL);
    if (recvd < 0)
      die("recvfrom");
    if ((size_t)recvd != size) {
      fprintf(stderr, "short recv: %zd\n", recvd);
      return 1;
    }

    uint64_t t1 = nsec_now();
    uint64_t seq = 0;
    memcpy(&seq, buf, sizeof(seq));
    if (seq != i) {
      fprintf(stderr, "seq mismatch: got=%" PRIu64 " expected=%" PRIu64 "\n", seq, i);
      return 1;
    }
    rtts[i] = t1 - t0;
  }

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
          "\"path\":\"veth_udp\","
          "\"peer\":\"%s\","
          "\"port\":%d,"
          "\"size\":%zu,"
          "\"iters\":%" PRIu64 ","
          "\"timeout_ms\":%d,"
          "\"rtt_ns\":{\"min\":%" PRIu64 ",\"p50\":%" PRIu64 ",\"p90\":%" PRIu64 ",\"p99\":%" PRIu64
          ",\"max\":%" PRIu64 "}"
          "}\n",
          peer_ip, port, size, iters, timeout_ms, min, p50, p90, p99, max);

  if (out != stdout)
    fclose(out);
  free(rtts);
  free(buf);
  return 0;
}
