/* TODO: everything runs locally atm need to add real networking. workers are threads need to switch to actual socket connections */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <gmp.h>
#define MAX_WORKERS 8
#define DIGITS_PER_TERM 14.18

// TODO: double check this constant
#define C3_OVER_24 "10939058860032000"

typedef struct {
    mpz_t P, Q, B;
} bst_t;

typedef struct {
    long long a;
    long long b;
    bst_t result;
} worker_args_t;

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

// merge two adjacent ranges [a,mid) and [mid,b)
void bst_merge(bst_t *out, bst_t *l, bst_t *r) {
    mpz_t t1, t2;
    mpz_init(t1);
    mpz_init(t2);

    // P_out = P_l * B_r * Q_r + P_r * B_l * Q_l
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

        // B(k) = (6k-5)(2k-1)(6k-1)
        mpz_set_si(out->B, 6*k - 5);
        mpz_mul_si(out->B, out->B, 2*k - 1);
        mpz_mul_si(out->B, out->B, 6*k - 1);

        // Q(k) = k^3 * C3/24
        mpz_t c3;
        mpz_init(c3);
        mpz_set_str(c3, C3_OVER_24, 10);
        mpz_set_si(out->Q, k);
        mpz_pow_ui(out->Q, out->Q, 3);
        mpz_mul(out->Q, out->Q, c3);
        mpz_clear(c3);

        // P(k) = (13591409 + 545140134*k) * B(k)
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

// TODO: replace with actual socket send/recv to a remote worker process
void *worker_thread(void *arg) {
    worker_args_t *w = (worker_args_t *)arg;
    bst_init(&w->result);
    bs(w->a, w->b, &w->result);
    printf("worker done: k=[%lld, %lld)\n", w->a, w->b);
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: %s <digits>\n", argv[0]);
        return 1;
    }

    int digits = atoi(argv[1]);
    int n_workers = 4; // TODO: make able to change

    long long n_terms = (long long)ceil((digits + 10) / DIGITS_PER_TERM) + 1;
    printf("digits=%d  terms=%lld  workers=%d\n", digits, n_terms, n_workers);

    // TODO: case where n_terms < n_workers
    long long chunk = n_terms / n_workers;
    worker_args_t workers[MAX_WORKERS];
    pthread_t threads[MAX_WORKERS];

    for (int i = 0; i < n_workers; i++) {
        workers[i].a = i * chunk;
        workers[i].b = (i == n_workers - 1) ? n_terms : (i + 1) * chunk;
        pthread_create(&threads[i], NULL, worker_thread, &workers[i]);
    }

    for (int i = 0; i < n_workers; i++)
        pthread_join(threads[i], NULL);

    // tree reduce the partial results
    bst_t results[MAX_WORKERS];
    for (int i = 0; i < n_workers; i++)
        results[i] = workers[i].result;

    int n = n_workers;
    while (n > 1) {
        int next = 0;
        for (int i = 0; i < n; i += 2) {
            if (i + 1 < n) {
                bst_t merged;
                bst_init(&merged);
                bst_merge(&merged, &results[i], &results[i+1]);
                bst_clear(&results[i]);
                bst_clear(&results[i+1]);
                results[next++] = merged;
            } else {
                results[next++] = results[i];
            }
        }
        n = next;
    }

    // pi = (426880 * sqrt(10005) * Q) / (13591409 * Q + P)
    mp_bitcnt_t prec = (mp_bitcnt_t)ceil(digits * 3.32193) + 64;
    mpf_set_default_prec(prec);

    mpf_t fP, fQ, sqrtD, num, den, pi;
    mpf_init(fP); mpf_init(fQ);
    mpf_init(sqrtD); mpf_init(num); mpf_init(den); mpf_init(pi);

    mpf_set_z(fP, results[0].P);
    mpf_set_z(fQ, results[0].Q);

    mpf_set_ui(sqrtD, 10005);
    mpf_sqrt(sqrtD, sqrtD);

    mpf_mul_ui(num, sqrtD, 426880);
    mpf_mul(num, num, fQ);
    mpf_mul_ui(den, fQ, 13591409);
    mpf_add(den, den, fP);
    mpf_div(pi, num, den);

    // TODO: write full result to file
    gmp_printf("pi = %.50Ff\n", pi);

    mpf_clear(fP); mpf_clear(fQ); mpf_clear(sqrtD);
    mpf_clear(num); mpf_clear(den); mpf_clear(pi);
    bst_clear(&results[0]);

    return 0;
}
