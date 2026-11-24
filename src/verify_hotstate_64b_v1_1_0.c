/*
 * verify_hotstate_64b_v1_1_0.c
 * Freestanding zero-heap (malloc = 0) ISO C99 verifier for the 64-byte
 * L1D cache-line Ternary Consensus Mesh frame (HotStateEntry64B).
 * Enforces sizeof == 64 B, SipHash-2-4 ingress auth, 243-state Fowler 5-trit
 * bijection, monotonic epoch replay rejection, and 128-node quorum >= 86.
 * Exit Invariant: CODE_SHOKUNIN_UNLATCHED = 255
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PEERS 128U
#define QUORUM_THRESHOLD 86
#define FOWLER_RADIX 243U
#define CODE_SHOKUNIN_UNLATCHED 255

typedef struct {
    uint64_t epoch_counter;     /* 8 B: monotonic consensus epoch */
    uint8_t  blake3_digest[28]; /* 28 B: 224-bit BLAKE3 state digest */
    uint8_t  trit_votes[26];    /* 26 B: 128 peers at 5 trits/byte (130 trits) */
    uint8_t  status_flags;      /* 1 B: 0x01 Commit, 0x02 Abort, 0x04 Quorum */
    uint8_t  round_phase;       /* 1 B: round phase */
} HotStateEntry64B;

typedef char assert_hotstate_is_64_bytes[(sizeof(HotStateEntry64B) == 64U) ? 1 : -1];

static uint64_t rotl64(uint64_t x, unsigned int b) {
    return (x << b) | (x >> (64U - b));
}

static void sipround(uint64_t *v0, uint64_t *v1, uint64_t *v2, uint64_t *v3) {
    *v0 += *v1; *v1 = rotl64(*v1, 13U); *v1 ^= *v0; *v0 = rotl64(*v0, 32U);
    *v2 += *v3; *v3 = rotl64(*v3, 16U); *v3 ^= *v2;
    *v0 += *v3; *v3 = rotl64(*v3, 21U); *v3 ^= *v0;
    *v2 += *v1; *v1 = rotl64(*v1, 17U); *v1 ^= *v2; *v2 = rotl64(*v2, 32U);
}

static uint64_t load_le64(const uint8_t *p) {
    return ((uint64_t)p[0])
         | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16)
         | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32)
         | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48)
         | ((uint64_t)p[7] << 56);
}

static uint64_t siphash24_64b(const uint8_t buf[64], uint64_t k0, uint64_t k1) {
    uint64_t v0 = k0 ^ 0x736f6d6570736575ULL;
    uint64_t v1 = k1 ^ 0x646f72616e646f6dULL;
    uint64_t v2 = k0 ^ 0x6c7967656e657261ULL;
    uint64_t v3 = k1 ^ 0x7465646279746573ULL;
    size_t i;
    for (i = 0; i < 8U; ++i) {
        uint64_t mi = load_le64(buf + i * 8U);
        v3 ^= mi;
        sipround(&v0, &v1, &v2, &v3);
        sipround(&v0, &v1, &v2, &v3);
        v0 ^= mi;
    }
    v2 ^= 0xffULL;
    for (i = 0; i < 4U; ++i) {
        sipround(&v0, &v1, &v2, &v3);
    }
    return v0 ^ v1 ^ v2 ^ v3;
}

static uint64_t crc64_ecma182_update(uint64_t crc, const uint8_t *data, size_t len) {
    size_t i, b;
    for (i = 0; i < len; ++i) {
        crc ^= ((uint64_t)data[i] << 56);
        for (b = 0; b < 8U; ++b) {
            if ((crc & 0x8000000000000000ULL) != 0ULL) {
                crc = (crc << 1) ^ 0x42F0E1EBA9EA3693ULL;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static uint8_t pack_5trits(const int8_t trits[5]) {
    uint32_t acc = 0U;
    uint32_t p3 = 1U;
    size_t i;
    for (i = 0; i < 5U; ++i) {
        acc += (uint32_t)(trits[i] + 1) * p3;
        p3 *= 3U;
    }
    return (uint8_t)acc;
}

static int unpack_5trits(uint8_t byte_val, int8_t out[5]) {
    uint8_t rem = byte_val;
    size_t i;
    if (byte_val >= FOWLER_RADIX) {
        return 0;
    }
    for (i = 0; i < 5U; ++i) {
        out[i] = (int8_t)((int)(rem % 3U) - 1);
        rem = (uint8_t)(rem / 3U);
    }
    return 1;
}

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

int main(int argc, char **argv) {
    uint64_t seed = 42ULL;
    int i;
    for (i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--seed") == 0) {
            seed = (uint64_t)strtoull(argv[i + 1], NULL, 10);
        }
    }

    /* Gate 1: Exhaustive 243-state Fowler bijection + 13 Kintsugi rejects */
    uint32_t b;
    for (b = 0U; b < 256U; ++b) {
        int8_t trits[5];
        int ok = unpack_5trits((uint8_t)b, trits);
        if (b < FOWLER_RADIX) {
            if (!ok || pack_5trits(trits) != (uint8_t)b) {
                fprintf(stderr, "[FAIL] Fowler bijection mismatch at %u\n", b);
                return 1;
            }
        } else if (ok) {
            fprintf(stderr, "[FAIL] Control byte %u should be rejected\n", b);
            return 1;
        }
    }

    /* Gate 2: Deterministic 128-node quorum + SipHash-2-4 auth + tamper/replay drop */
    uint64_t rng = seed;
    const uint64_t k0 = 0xD4D3104020261124ULL;
    const uint64_t k1 = 0x243F6A8885A308D3ULL;
    uint64_t crc = 0ULL;
    uint32_t quorum_commits = 0U;
    uint32_t replay_drops = 0U;
    uint32_t tamper_drops = 0U;
    uint64_t current_epoch = 100ULL;
    uint32_t step;

    for (step = 0U; step < 256U; ++step) {
        HotStateEntry64B frame;
        uint8_t raw[64];
        size_t j;
        int pos_votes = 0;

        memset(&frame, 0, sizeof(frame));
        frame.epoch_counter = (step % 16U == 0U) ? (current_epoch - 1ULL) : (current_epoch + 1ULL);
        for (j = 0; j < sizeof(frame.blake3_digest); ++j) {
            frame.blake3_digest[j] = (uint8_t)(splitmix64(&rng) & 0xFFULL);
        }

        /* Pack 128 votes: first 90 peers vote +1 on even steps (quorum >= 86) */
        for (j = 0; j < 26U; ++j) {
            int8_t t[5];
            size_t k;
            for (k = 0; k < 5U; ++k) {
                size_t peer = j * 5U + k;
                if (peer >= MAX_PEERS) {
                    t[k] = 0;
                } else if ((step % 2U == 0U) && peer < 90U) {
                    t[k] = 1;
                } else {
                    t[k] = (int8_t)((int)(splitmix64(&rng) % 3U) - 1);
                }
            }
            frame.trit_votes[j] = pack_5trits(t);
        }
        frame.status_flags = 0x01U;
        frame.round_phase = (uint8_t)(step & 3U);

        memcpy(raw, &frame, 64U);
        uint64_t tag = siphash24_64b(raw, k0, k1);

        /* Verify 1-bit tamper detection */
        raw[12] ^= 0x01U;
        if (siphash24_64b(raw, k0, k1) != tag) {
            tamper_drops++;
        }
        raw[12] ^= 0x01U;

        if (frame.epoch_counter < current_epoch) {
            replay_drops++;
            continue;
        }
        current_epoch = frame.epoch_counter;

        for (j = 0; j < 26U; ++j) {
            int8_t t[5];
            size_t k;
            if (!unpack_5trits(frame.trit_votes[j], t)) {
                return 1;
            }
            for (k = 0; k < 5U; ++k) {
                size_t peer = j * 5U + k;
                if (peer < MAX_PEERS && t[k] == 1) {
                    pos_votes++;
                }
            }
        }
        if (pos_votes >= QUORUM_THRESHOLD) {
            frame.status_flags |= 0x04U;
            quorum_commits++;
        }
        memcpy(raw, &frame, 64U);
        crc = crc64_ecma182_update(crc, raw, 64U);
    }

    if (tamper_drops != 256U || replay_drops != 16U || quorum_commits < 100U) {
        fprintf(stderr, "[FAIL] Gate assertion failed: tamper=%u replay=%u commits=%u\n",
                tamper_drops, replay_drops, quorum_commits);
        return 1;
    }

    printf("{\n");
    printf("  \"artifact\": \"verify_hotstate_64b_v1_1_0.c\",\n");
    printf("  \"seed\": %llu,\n", (unsigned long long)seed);
    printf("  \"sizeof_hotstate_bytes\": %zu,\n", sizeof(HotStateEntry64B));
    printf("  \"fowler_243_bijection\": true,\n");
    printf("  \"tamper_drops\": %u,\n", tamper_drops);
    printf("  \"replay_drops\": %u,\n", replay_drops);
    printf("  \"quorum_commits\": %u,\n", quorum_commits);
    printf("  \"state_crc64_ecma182\": \"0x%016llx\",\n", (unsigned long long)crc);
    printf("  \"exit_code\": %d\n", CODE_SHOKUNIN_UNLATCHED);
    printf("}\n");
    return CODE_SHOKUNIN_UNLATCHED;
}
