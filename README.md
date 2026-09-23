# About the fork
This fork includes merged and unmerged code intended for upstreaming
or experimentation.

## Branch summaries

### **accelerate fd_hash via AVX-512 256-bit registers**
Shaved ~22% from `fd_hash` under stable profiling with hand-written
SIMD. The implementation performs best on `32n`-sized inputs, and is
slower on tails of the form `32n + x` (`0 < x < 32`), while still
beating the baseline.
See [source code](https://github.com/0xfka/firedancer/blob/fd-hash-avx512/src/util/fd_hash.c), [PR 10575](https://github.com/firedancer-io/firedancer/pull/10575), and [PR 10620](https://github.com/firedancer-io/firedancer/pull/10620).

### **Zstandard frame scanner**
A helper that finds frame start/size without decompressing, enables
snapshot loading pipeline to run concurrently. Coverage included
fuzzing with seeds from [Zstandard sample files by mcraiha](https://github.com/mcraiha/ZSTD-sample-files).

### **Experiments on snapshot-create pipeline**
Global sorting by owner pubkey (key 1) and mint (key 2) reduced snapshot
sizes by up to 20%+ versus Firedancer output in tests. This branch
focused on staging/queueing and did not include sorting. 
Adding staging/queueing buffers in `FD_BACKUP_ORIG_ACC_DISK_BATCH`
increased cache pressure (`QUEUE_BUF_SZ_MINIMUM` ), and benchmarks
showed reduced compression time (~6%) but no meaningful size win
(`120 GB` to `118 GB`) and increased latency on a 1 MiB L2 system.
See https://github.com/0xfka/firedancer/tree/snapshot_wip for source
code.

### **Snapshot-server slow-peer protection**
The `snap_slowloris` branch adds a per-connection throughput guard to
`snapsv`: downloads must sustain 3 MiB/s over 10-second windows by
default, or the connection is aborted. Unit coverage includes both a
legitimate stream and a slow peer.

Minimum clean L7 traffic to hold all slots is approximately
`conn_max × 3 MiB/s` per validator (before protocol overhead and
without relying on external rate limiting, e.g. a CDN): `384 MiB/s`
for 128 connections, or `300,000 MiB/s`
(~293 GiB/s, ~2.52 Tb/s) for 100,000 connections.
