# Ternary Consensus Mesh: Line-Rate Post-Quantum Byzantine Consensus

This repository contains the reference eBPF/XDP drivers, Triton GPU lookup kernels, and microbenchmarking suites for the paper:

> **"Radix Economy and Balanced Ternary Microarchitectures: Resolving the Memory Wall in Line-Rate Post-Quantum Consensus and Nanoscale Computing"**  
> *Target Venues: SOSP / OSDI / ISCA / ASPLOS*

---

## Key Technical Highlights

1. **64-Byte L1D Cache-Resident Frame**: Compresses a 128-node consensus vote bitmask to 26 bytes ($3^5 = 243 \le 256$), enabling the entire synchronous descriptor (epoch + BLAKE3 accumulator + status flags) to fit within exactly one 64-byte L1D cache line.
2. **Deterministic Wire Latency**: Evaluated via eBPF XDP at 100GbE line-rate (99.2 Mpps aggregate across 8 queues, 12.4 Mpps/core) with $p50 = 40.0\text{ ns}$ and $p99.9 = 60.0\text{ ns}$, safely below the $80.5\text{ ns}$ frame budget.
3. **State-Crypt Separation**: Decouples the 64-byte synchronous consensus frame from asynchronous ML-DSA-44 (NIST FIPS 204) signature verification offloaded over 2MB hugepage lock-free SPSC rings.
4. **Conflict-Free GPU Decompression**: Triton kernel maps 5-trit packed bytes into FP16 ternary weights with zero shared-memory bank conflicts using single-cycle hardware broadcast addressing.

---

## Repository Structure

* `xdp_ternary_filter.c` - Production eBPF XDP C driver for line-rate packet parsing, SipHash-2-4 pre-authentication, monotonic epoch tracking, and fast-path quorum accumulation.
* `triton_lut_kernel.py` - Triton GPU kernel for high-throughput 5-trit decompression on Tensor Cores.
* `benchmark_harness.py` - Microarchitectural evaluation reproducing the latency and throughput ablations across 64-byte ternary and 96-byte binary frames.
* `LICENSE` - MIT License.

---

## Build & Usage Instructions

### 1. Compile the eBPF XDP Filter
```bash
# Requires clang and libbpf
clang -O2 -target bpf -c xdp_ternary_filter.c -o xdp_ternary_filter.o

# Attach to your 100GbE network interface (e.g. eth0) in native XDP mode
ip link set dev eth0 xdpgeneric obj xdp_ternary_filter.o sec xdp
```

### 2. Run the Triton GPU Decompression Kernel
```bash
# Requires PyTorch and Triton
python3 triton_lut_kernel.py
```

### 3. Run the Microbenchmarking Suite
```bash
python3 benchmark_harness.py
```

---

## Citation
```bibtex
@article{grimm2026radix,
  title={Radix Economy and Balanced Ternary Microarchitectures: Resolving the Memory Wall in Line-Rate Post-Quantum Consensus and Nanoscale Computing},
  author={Grimm, Justin},
  year={2026}
}
```

## License
MIT License - Copyright (c) 2026 Justin Grimm.