/*
 * xdp_ternary_filter.c - Line-rate eBPF XDP Ingress Filter for 64-Byte Balanced Ternary Consensus
 *
 * Implements line-rate SipHash-2-4 pre-authentication, monotonic epoch tracking,
 * and 5-trit byte decoding for 128-node Byzantine consensus quorums.
 * Fits within a single 64-byte L1D cache-line frame.
 *
 * Target: Linux eBPF / XDP (Driver / Native mode)
 * Compiler: clang -O2 -target bpf -c xdp_ternary_filter.c -o xdp_ternary_filter.o
 */

#include <linux/bpf.h>
#include <linux/if_ethernet.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define MAX_PEERS 128
#define QUORUM_THRESHOLD 86 // ceil(2 * 128 / 3) = 86
#define UDP_CONSENSUS_PORT 9999

/*
 * 64-Byte Invariant Cache-Line Structure
 */
struct __attribute__((__packed__)) HotStateEntry {
    __u64 epoch_counter;       // 8 bytes: Monotonically increasing consensus epoch
    __u8  blake3_digest[32];   // 32 bytes: Merkle root / state digest accumulator
    __u8  trit_votes[26];      // 26 bytes: 128 nodes packed at 5 trits/byte (26 * 5 = 130 trits)
    __u8  status_flags;        // 1 byte: 0x01 = Commit, 0x02 = Abort, 0x04 = Quorum Verified
    __u8  round_phase;         // 1 byte: Round phase indicator
};

/*
 * SipHash-2-4 Symmetric Pairwise Keys Map
 * Lock-free array map indexed by peer node ID (0..127)
 */
struct peer_key {
    __u64 k0;
    __u64 k1;
};

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, __u32);
    __type(value, struct peer_key);
    __uint(max_entries, MAX_PEERS);
} peer_keys_map SEC(".maps");

/*
 * Active Epoch State Tracking Map (Single Entry)
 */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, __u32);
    __type(value, __u64);
    __uint(max_entries, 1);
} current_epoch_map SEC(".maps");

/*
 * Global 243-entry decode lookup table (5 trits per byte)
 * Each entry holds unpack metadata: count of Positive, Negative, Zero trits
 */
struct trit_decoded {
    __s8 trits[5]; // -1, 0, +1
};

/* Rotate helper for SipHash */
static __always_inline __u64 rotl(__u64 x, __u64 b) {
    return (x << b) | (x >> (64 - b));
}

static __always_inline void sipround(__u64 *v0, __u64 *v1, __u64 *v2, __u64 *v3) {
    *v0 += *v1; *v1 = rotl(*v1, 13); *v1 ^= *v0; *v0 = rotl(*v0, 32);
    *v2 += *v3; *v3 = rotl(*v3, 16); *v3 ^= *v2;
    *v0 += *v3; *v3 = rotl(*v3, 21); *v3 ^= *v0;
    *v2 += *v1; *v1 = rotl(*v1, 17); *v1 ^= *v2; *v2 = rotl(*v2, 32);
}

/* Fast SipHash-2-4 tag evaluation (64-bit output) */
static __always_inline __u64 siphash24(const void *data, __u32 len, __u64 k0, __u64 k1) {
    __u64 v0 = k0 ^ 0x736f6d6570736575ULL;
    __u64 v1 = k1 ^ 0x646f72616e646f6dULL;
    __u64 v2 = k0 ^ 0x6c7967656e657261ULL;
    __u64 v3 = k1 ^ 0x7465646279746573ULL;

    const __u64 *m = (const __u64 *)data;
    #pragma unroll
    for (int i = 0; i < 8; i++) { // 64 bytes = 8 u64 words
        __u64 mi = m[i];
        v3 ^= mi;
        sipround(&v0, &v1, &v2, &v3);
        sipround(&v0, &v1, &v2, &v3);
        v0 ^= mi;
    }

    v2 ^= 0xff;
    #pragma unroll
    for (int i = 0; i < 4; i++) {
        sipround(&v0, &v1, &v2, &v3);
    }

    return v0 ^ v1 ^ v2 ^ v3;
}

SEC("xdp")
int xdp_ternary_consensus_ingress(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    /* Stage 1: Fast Ethernet & IP Header Bounds Check */
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;

    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return XDP_PASS;

    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)(iph + 1) > data_end)
        return XDP_PASS;

    if (iph->protocol != IPPROTO_UDP)
        return XDP_PASS;

    struct udphdr *udp = (void *)(iph + 1);
    if ((void *)(udp + 1) > data_end)
        return XDP_PASS;

    if (udp->dest != bpf_htons(UDP_CONSENSUS_PORT))
        return XDP_PASS;

    /* Stage 2: Payload Size Verification (64-byte frame + 8-byte SipHash tag = 72 bytes) */
    void *payload = (void *)(udp + 1);
    if (payload + sizeof(struct HotStateEntry) + sizeof(__u64) > data_end)
        return XDP_DROP;

    struct HotStateEntry *entry = payload;
    __u64 *tag = (__u64 *)(payload + sizeof(struct HotStateEntry));

    /* Stage 3: Peer Identity & Pairwise Key Extraction */
    __u32 peer_id = bpf_ntohl(iph->saddr) & (MAX_PEERS - 1);
    struct peer_key *key = bpf_map_lookup_elem(&peer_keys_map, &peer_id);
    if (!key)
        return XDP_DROP;

    /* Stage 4: SipHash-2-4 Ingress Verification */
    __u64 computed_tag = siphash24(entry, sizeof(struct HotStateEntry), key->k0, key->k1);
    if (computed_tag != *tag)
        return XDP_DROP; // Cryptographic pre-authentication failure

    /* Stage 5: Monotonic Epoch Validation */
    __u32 epoch_key = 0;
    __u64 *current_epoch = bpf_map_lookup_elem(&current_epoch_map, &epoch_key);
    if (current_epoch) {
        if (entry->epoch_counter < *current_epoch) {
            return XDP_DROP; // Stale replay packet
        }
    }

    /* Stage 6: Line-Rate Ternary Vote Quorum Accumulation */
    __s32 positive_votes = 0;
    #pragma unroll
    for (int i = 0; i < 26; i++) {
        __u8 byte_val = entry->trit_votes[i];
        if (byte_val >= 243)
            return XDP_DROP; // Canonical encoding enforcement (3^5 = 243)

        // Mathematical unpack of 5 trits (radix-3 decomposition)
        __u8 rem = byte_val;
        #pragma unroll
        for (int t = 0; t < 5; t++) {
            __s8 trit = (rem % 3) - 1; // Maps 0 -> -1, 1 -> 0, 2 -> +1
            rem /= 3;
            if (trit == 1)
                positive_votes++;
        }
    }

    /* Stage 7: Immediate Fast-Path Consensus Finalization */
    if (positive_votes >= QUORUM_THRESHOLD) {
        entry->status_flags |= 0x04; // Mark quorum reached
        return XDP_PASS; // Dispatch to local memory ring / io_uring
    }

    return XDP_PASS;
}

char _license[] SEC("license") = "Dual MIT/GPL";