/* See LICENSE file for copyright and license details. */
/* Mirrors dwmr/src/util.rs */

#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))
#define LENGTH(X)               (sizeof (X) / sizeof (X)[0])

void die(const char *fmt, ...) __attribute__((noreturn));
void *ecalloc(size_t nmemb, size_t size);
void truncate_utf8(char *dst, const char *src, size_t size);
