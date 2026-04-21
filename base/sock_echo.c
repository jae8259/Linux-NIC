#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static void die(const char *msg) {
  perror(msg);
  exit(1);
}

static void usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s --bind <ip> --port <port>\n"
          "  UDP echo server for veth baseline.\n",
          prog);
  exit(2);
}

static bool streq(const char *a, const char *b) { return strcmp(a, b) == 0; }

int main(int argc, char **argv) {
  const char *bind_ip = "0.0.0.0";
  int port = 7777;

  for (int i = 1; i < argc; i++) {
    if (streq(argv[i], "--bind") && i + 1 < argc) {
      bind_ip = argv[++i];
    } else if (streq(argv[i], "--port") && i + 1 < argc) {
      port = atoi(argv[++i]);
    } else if (streq(argv[i], "-h") || streq(argv[i], "--help")) {
      usage(argv[0]);
    } else {
      usage(argv[0]);
    }
  }

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    die("socket");

  int one = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
    die("setsockopt(SO_REUSEADDR)");

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, bind_ip, &addr.sin_addr) != 1) {
    fprintf(stderr, "invalid --bind ip: %s\n", bind_ip);
    return 2;
  }

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    die("bind");

  uint8_t buf[65536];
  while (1) {
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&peer, &peer_len);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      die("recvfrom");
    }

    ssize_t sent = sendto(fd, buf, (size_t)n, 0, (struct sockaddr *)&peer, peer_len);
    if (sent < 0) {
      if (errno == EINTR)
        continue;
      die("sendto");
    }
  }
}

