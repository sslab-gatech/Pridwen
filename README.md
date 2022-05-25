# Pridwen
Universally Hardening SGX Programs via Load-Time Synthesis

(The following artifact description replicates the description in section 6 Evaluation of the paper.)

We evaluate PRIDWEN on successful mitigation of individual targeted SCA , the semantic correctness of the input Wasm program, the performance characteristics of the PRIDWEN loader, and the performance overhead of PRIDWEN-synthesized binaries.

Experiment setup are as follows. We ran all the experiments on a machine with a 4-core Intel i7-6700K CPU (Skylake microarchitecture) operating at 4 GHz with 32 KiB L1 and 256 KiB L2 private caches, an 8 MiB L3 shared cache, and 64 GiB of RAM. The machine was running Linux kernel 4.15. The PRIDWEN loader is compiled with gcc 5.4.0 and executed on top of the Intel Linux SGX SDK 2.5.102.

The applications and test suites we use are as follows. We use three real-world applications or libraries (Lighttpd 1.4.48, libjpeg 9a, and SQLite 3.21.0) as a macro-benchmark suite representing large, complex applications, as well as a micro-benchmark suite, PolyBenchC. The benchmark suite consists of 23 small C programs with only numerical computations (i.e., no syscall) that are used to evaluate the runtime performance of just-in-time compiled Wasm binaries against native C binaries. We compile the original source code of each micro- or macro-benchmark program into Wasm using Emscripten, an LLVM-based compiler. We also directly port all of the programs using SGX SDK to serve as baseline versions. We use the official Wasm specification test suite to test the correctness of the synthesis of PRIDWEN.