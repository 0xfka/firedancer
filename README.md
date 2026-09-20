# About the fork
This fork includes merged/unmerged and codes that meant to be upstream or experimental.
For all my sent PR's ( including drafts meant for a quick review from maintainers), see [github PR section of firedancer, filtered by author](https://github.com/firedancer-io/firedancer/pulls?q=is%3Apr+author%3A0xfka)

A quick summary of the work:

### **accelerate fd_hash via AVX-512 256 bit registers**
Shaved ~22% from fd_hash function under stable profiling with hand written SIMD, at multiple of 32 path, which means the implementation performs way better on multiples of 32, and worse than that at multiple of 32 + x(where x < 32) but still better than the baseline.
see [github](https://github.com/0xfka/firedancer/blob/fd-hash-avx512/src/util/fd_hash.c) for source code, [pr 10575](https://github.com/firedancer-io/firedancer/pull/10575) for benchmark harness I coded for it so I can optimize, [pr 10620](https://github.com/firedancer-io/firedancer/pull/10620) for how I profiled the code and the detailed results.

###  **Zstandard frame scanner**
The Firedancer project required a helper function that needs to find start/size of the frames without decompressing, allowing snapshot loading pipeline to run concurrently.
Covered every case with fuzzing, with a seed generated from [Zstandard sample files from "mcraiha"](https://github.com/mcraiha/ZSTD-sample-files).

Can be tested with:
```console
git clone git@github.com:0xfka/firedancer.git
cd firedancer
git checkout frame_wip # Named wip when I needed to send it as a draft PR. Works reliably.
make -j$(nproc) test_zstd
./build/native/gcc/unit-test/test_zstd
```
And start/sizes are printed. The source code can definitely be ported into a streaming, returning value, or similar patterns.

A dummy file generated with Linux's /dev/urandom and provided at project root, named "multi_frame_hundreds.zst", can be changed. The code returns 0 on error, and frame count if not.

Because usual error paths we had at that parser can cause segfaults or reading garbage, which can create a malformed state, *Flag based error handling ( setting a flag on error and check if( flag==1 ) only once )* is not possible.
Cmov is a data-flow instruction, not a control-flow instruction, so it requires both paths must be safe to evaluate (e.g. no jump/returns, no potential segfaults or similar, etc.), which ended up requiring actual branches with unlikely compiler hints.

This branch was superseded due to overlapping work upstream.

Note that this code is not tuned or profiled because it is only used at snapshot load and the possible performance gains may coming from the single/multi core difference, not from micro optimizations.

This branch may be updated with streaming support at scanner + concurrency with benchmarks, showing upstream single core as baseline.

### **Experiments on snapshot-create pipeline**
After finding out global sorting based on owner pubkey as key 1 and mint as key 2 was shaving the sizes up to 20%+ from firedancer's size, which even passes agave, I hit
hardware limitations when trying to implement it to the upstream production pipeline :
First, I added staging & queueing with stage buffers inside FD_BACKUP_ORIG_ACC_DISK_BATCH, which is responsible for highest % of the accdb at snapzp, and was already parsing the accounts, which means lower overhead than others.
Because 1 staging buffer must be at least the size of
header + max data len this function can receive ( also keep in mind the aligning ), 1 buffer must be at least
262208 bytes ( see QUEUE_BUF_SZ_MINIMUM ), and we need at least 2 of them, 524416 bytes, which exceeds many L2 caches on market, or at least uses a huge partition of it. After all, this queueing didn't reduce the snapshot size on a meaningful way ( 118 gb from 120 )
and increased latency on a server that has 1 mb l2 ( amd epyc 9004, zen 5 ), even though compression time was ~6% lower.
This direction was explored based on discussions with a Firedancer maintainer and subsequently shelved after benchmarks showed the cache overhead outweighed the benefits.
See https://github.com/0xfka/firedancer/tree/snapshot_wip for source code.

### **Snapshot-server slow-peer protection**
The `snap_slowloris` branch adds a per-connection throughput guard to `snapsv`: downloads must sustain 3 MiB/s over 10-second windows by default, or the connection is aborted. Unit coverage includes both a legitimate stream and a slow peer.

The minimum clean L7 traffic needed to hold all slots is approximately:
`conn_max × 3 MiB/s` per validator (before protocol overhead and without a CDN). That is 384 MiB/s for 128 connections, or 300,000 MiB/s (~293 GiB/s, ~2.52 Tb/s) for 100,000 connections.

### here goes the upstream Firedancer readme.md :


# [Firedancer](https://jumpcrypto.com/firedancer/) 🔥💃

Firedancer is a new validator client for Solana.

* **Fast** Designed from the ground up to be *fast*. The concurrency
model draws from experience in the low latency trading space, and the code
contains many novel high-performance reimplementations of core Solana
primitives.
* **Secure** The architecture of the validator allows it to run with a
highly restrictive sandbox and almost no system calls.
* **Independent** Firedancer is written from scratch. This brings client
diversity to the Solana network and helps it stay resilient to supply
chain attacks in build tooling or dependencies.

## Documentation
If you are an operator or looking to run the validator, see the Getting
Started guide in the [Firedancer
docs](https://docs.firedancer.io/)

## Releases
If you are an operator looking to run the validator, see the [Releases
Guide](https://docs.firedancer.io/guide/getting-started.html#releases)
in the documentation.

The Firedancer project is producing two validators,

* **Frankendancer** A hybrid validator using parts of Firedancer and
parts of Agave. Frankendancer uses the Firedancer networking stack and
block production components to perform better while leader. Other
functionality including execution and consensus is using the Agave
validator code.
* **Firedancer** A full from-scratch Firedancer with no Agave code.

Both validators are built from this codebase. The Firedancer validator
is not ready for test or production use and has no releases.
Frankendancer is currently available on both Solana testnet and
mainnet-beta.

## Developing
Firedancer currently only supports Linux and requires a relatively new
kernel, at least v4.18 to build.

```console
$ git clone https://github.com/firedancer-io/firedancer.git
$ cd firedancer
$ ./deps.sh
$ source activate  # enter build environment
$ make -j

# Run a new development cluster
$ firedancer-dev

# Join Solana testnet
$ firedancer-dev --testnet
```

`firedancer-dev` (without args) configures your system for validator
operation and creates a new lcoal development cluster. First it creates
a genesis block, some keys, a faucet, and then it starts a validator on
the local machine. `firedancer-dev` will use `sudo` to make privileged
changes to system configuration where needed. If `sudo` is not available,
you may need to run the command as root.

If you wish to join this cluster with other validators, you can define
`[gossip.entrypoints]` in the configuration file to point at your first
validator and join with `firedancer-dev run`.

## License
Firedancer is available under the [Apache 2
license](https://www.apache.org/licenses/LICENSE-2.0). Firedancer also
includes external libraries that are available under a variety of
licenses. See [LICENSE](LICENSE) for the full license text.
