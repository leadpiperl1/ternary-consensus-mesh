# Ternary Consensus Mesh (`v1.1.0`): Line-Rate Post-Quantum Byzantine Consensus

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.22851921.svg)](https://doi.org/10.5281/zenodo.22851921)
[![SWAR Companion DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.24510014.svg)](https://doi.org/10.5281/zenodo.24510014)
[![Waystation CI](https://img.shields.io/badge/Waystation_CI-S__runner%3D0_%C2%B7_14d_Cooldown-34D399?style=flat-square)](.github/workflows/waystation-ci.yml)
[![Exit Invariant: 255](https://img.shields.io/badge/CODE__SHOKUNIN__UNLATCHED-255-F59E0B?style=flat-square)](waystation_punchcard.json)

Reference eBPF/XDP driver, strict **64-byte L1D-aligned ISO C99 verifier (`malloc = 0`)**, Triton GPU lookup kernel, and **100% pure Python 3 stdlib** microbenchmarking harness for:

> **"Radix Economy and Balanced Ternary Microarchitectures: Resolving the Memory Wall in Line-Rate Post-Quantum Consensus and Nanoscale Computing"** ([DOI: 10.5281/zenodo.22851921](https://doi.org/10.5281/zenodo.22851921)) & **64-Trit SWAR POPCNT SUB-04 Substrate v2.0.0** ([DOI: 10.5281/zenodo.24510014](https://doi.org/10.5281/zenodo.24510014))

---

## Key Technical Highlights (`v1.1.0`)

1. **Exact 64-Byte L1D Cache-Resident Frame (`8 + 28 + 26 + 1 + 1 = 64 B`)**: Fixed `struct HotStateEntry` to exact 64-byte packing (`uint64_t epoch_counter` + 224-bit `blake3_digest[28]` + 128-node `trit_votes[26]` at 5 trits/byte + `status_flags` + `round_phase`), compile-time locked and verified on host via [`src/verify_hotstate_64b_v1_1_0.c`](src/verify_hotstate_64b_v1_1_0.c) (`malloc = 0`, `-std=c99 -Werror -pedantic`, `CODE_SHOKUNIN_UNLATCHED = 255`).
2. **Deterministic Wire Latency**: Pure-stdlib [`benchmark_harness.py`](benchmark_harness.py) (`--seed 42`, zero `numpy`) verifies 64-byte ternary p99.9 latency safely below the `80.5 ns` 100GbE frame budget vs. 96-byte binary PBFT cache-line splits.
3. **Waystation `wfx-loot` Immunity**: `S_runner = 0`, `permissions: contents: read`, SHA-pinned `actions/checkout` (`persist-credentials: false`), `zizmor`, and 14-day Dependabot cooldown.

---

## Quick Start (`<= 60s` Deterministic Verification)

```bash
make clean && make verify
```

### Optional eBPF/XDP & GPU Targets

```bash
# Requires clang and libbpf:
clang -O2 -target bpf -c xdp_ternary_filter.c -o xdp_ternary_filter.o

# Requires PyTorch and Triton:
python3 triton_lut_kernel.py
```

---

## License & Attribution

MIT License — Justin D. Grimm (Independent Dandelion Substrate Research Group, Gurley, Alabama 35748, USA · [ORCID: 0009-0000-5299-0243](https://orcid.org/0009-0000-5299-0243)) · `CODE_SHOKUNIN_UNLATCHED = 255`.
