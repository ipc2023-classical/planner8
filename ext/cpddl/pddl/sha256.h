/** Adapted from https://github.com/B-Con/crypto-algorithms.
 *  Author: Brad Conte (brad AT bradconte.com)
 *  Public domain.
 */

#ifndef _PDDL_SHA256_H_
#define _PDDL_SHA256_H_

#include <pddl/common.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define PDDL_SHA256_HASH_SIZE 32 // SHA256 outputs a 32 byte digest
#define PDDL_SHA256_HASH_STR_SIZE (64 + 1)

struct pddl_sha256 {
    unsigned char data[64];
    int datalen;
    unsigned long long bitlen;
    int state[8];
};
typedef struct pddl_sha256 pddl_sha256_t;

void pddlSHA256Init(pddl_sha256_t *ctx);
void pddlSHA256Update(pddl_sha256_t *ctx, const void *data, size_t datalen);
void pddlSHA256Finalize(pddl_sha256_t *ctx, void *hash);
void pddlSHA256(const void *data, size_t datalen, void *hash);
void pddlSHA256ToStr(const void *hash, char *hash_text);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _PDDL_SHA256_H_ */
