/* TODO: JSON parsing to proper parser, haven't tested this with more than one connection yet, need to handle partial sends */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <gmp.h>

#define BUFSIZE 4096
#define C3_OVER_24 "10939058860032000"

typedef struct {
    mpz_t P, Q, B;
} bst_t;

void bst_init(bst_t *t) {
    mpz_init(t->P);
    mpz_init(t->Q);
    mpz_init(t->B);
}

void bst_clear(bst_t *t) {
    mpz_clear(t->P);
    mpz_clear(t->Q);
    mpz_clear(t->B);
}

void bst_merge(bst_t *out, bst_t *l, bst_t *r) {
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    mpz_mul(t1, l->P, r->B);
    mpz_mul(t1, t1, r->Q);
    mpz_mul(t2, r->P, l->B);
    mpz_mul(t2, t2, l->Q);
    mpz_add(out->P, t1, t2);

    mpz_mul(out->Q, l->Q, r->Q);
    mpz_mul(out->B, l->B, r->B);

    mpz_clear(t1);
    mpz_clear(t2);
}

void bs(long long a, long long b, bst_t *out) {
    if (b == a + 1) {
        long long k = a;

        if (k == 0) {
            mpz_set_ui(out->P, 13591409);
            mpz_set_ui(out->Q, 1);
            mpz_set_ui(out->B, 1);
            return;
        }

        mpz_set_si(out->B, 6*k - 5);
        mpz_mul_si(out->B, out->B, 2*k - 1);
        mpz_mul_si(out->B, out->B, 6*k - 1);

        mpz_t c3;
        mpz_init(c3);
        mpz_set_str(c3, C3_OVER_24, 10);
        mpz_set_si(out->Q, k);
        mpz_pow_ui(out->Q, out->Q, 3);
        mpz_mul(out->Q, out->Q, c3);
        mpz_clear(c3);

        mpz_t linear;
        mpz_init(linear);
        mpz_set_str(linear, "545140134", 10);
        mpz_mul_si(linear, linear, k);
        mpz_add_ui(linear, linear, 13591409);
        mpz_mul(out->P, linear, out->B);
        mpz_clear(linear);

        if (k % 2 == 1)
            mpz_neg(out->P, out->P);

        return;
    }

    long long mid = (a + b) / 2;
    bst_t left, right;
    bst_init(&left);
    bst_init(&right);
    bs(a, mid, &left);
    bs(mid, b, &right);
    bst_merge(out, &left, &right);
    bst_clear(&left);
    bst_clear(&right);
}

// TODO: should use a real parser
long long parse_json_ll(const char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":", key);
    char *p = strstr(json, pat);
    if (!p) return -1;
    p += strlen(pat);
    return strtoll(p, NULL, 10);
}

void handle_client(int fd) {
    char buf[BUFSIZE];
    int n = recv(fd, buf, BUFSIZE - 1, 0);
    if (n <= 0) {
        printf("failed to read from client\n");
        return;
    }
    buf[n] = '\0';
    printf("received: %s\n", buf);

    long long a      = parse_json_ll(buf, "a");
    long long b      = parse_json_ll(buf, "b");
    // long long digits = parse_json_ll(buf, "digits");

    if (a < 0 || b <= a) {
        printf("bad range: a=%lld b=%lld\n", a, b);
        return;
    }

    printf("computing k=[%lld, %lld)\n", a, b);

    bst_t result;
    bst_init(&result);
    bs(a, b, &result);

    // encode as hex and send back
    // TODO: response can be huge, edit handing
    char *Phex = mpz_get_str(NULL, 16, result.P);
    char *Qhex = mpz_get_str(NULL, 16, result.Q);
    char *Bhex = mpz_get_str(NULL, 16, result.B);

    size_t resp_size = strlen(Phex) + strlen(Qhex) + strlen(Bhex) + 64;
    char *resp = malloc(resp_size);
    snprintf(resp, resp_size,
             "{\"P\":\"%s\",\"Q\":\"%s\",\"B\":\"%s\"}\n",
             Phex, Qhex, Bhex);

    // TODO: loop on send in case it doesn't send everything at once
    int sent = send(fd, resp, strlen(resp), 0);
    printf("sent %d bytes\n", sent);

    free(resp);
    free(Phex);
    free(Qhex);
    free(Bhex);
    bst_clear(&result);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 4) < 0) {
        perror("listen");
        return 1;
    }

    printf("worker listening on port %d\n", port);

    // TODO: only handles one connection
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("connection from %s\n", inet_ntoa(client_addr.sin_addr));
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
