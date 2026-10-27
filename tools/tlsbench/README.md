# TLS on the Falcon: measurements

Can the Falcon do TLS itself, without the Raspberry Pi? These two programs
time the expensive parts of a TLS handshake on the machine itself.

- `TLSB030.PRG`: BearSSL 0.6 compiled for the 68030 times X25519, P-256,
  ECDSA and RSA-2048 signature checks, SHA-256, AES-GCM and
  ChaCha20-Poly1305.
- `RSA030.PRG`: the RSA public operation (a signature check) with its
  Montgomery multiplication in 68030 assembly (`mont030.S`, `rsa030.c`),
  against BearSSL's i31: the same answers, and the times. `rsacheck`
  checks `rsa030.c` against BearSSL on the build machine.
- `ECBENCHC.PRG`, `ECBENCHA.PRG`: RSA, P-256, P-384, ECDSA and X25519 with
  BearSSL's `br_i31_montymul` in C and in 68030 assembly.
- `TLSDSP.PRG`: Montgomery multiplication (the core of RSA and elliptic
  curves) on the DSP56001, for 2048-bit and 256-bit numbers. It checks the
  DSP's answers against values worked out in Python (`gen.py`).

Each shows its results and saves them to `TLSBENCH.TXT` / `TLSDSP.TXT`.
Run `build.sh` to rebuild them (needs `a56`, see the script).

## Emulated Falcon (Hatari, 68030 16 MHz, cycle-exact; DSP emulated)

| | 68030 (BearSSL C) | DSP56001 |
|---|---|---|
| RSA-2048 signature check | 2515 ms | about 60 ms (19 x 3.15 ms) |
| X25519 | 1070 ms | about 180 ms (estimate: 2550 x 72 us) |
| ECDSA P-256 check | 7975 ms | not measured |
| Handshake, 2 x X25519 + 3 RSA checks | about 9.6 s | about 0.5 s |
| ChaCha20-Poly1305 / AES-128-GCM | 39 / 16 KB/s | |

Hatari runs the DSP twice as fast as the real one.

## Real Falcon, 68030 at 50 MHz on a DFB1X board, with fast RAM

| | 68030 in ST-RAM | 68030 in fast RAM | DSP56001 |
|---|---|---|---|
| X25519 | 1545 ms | 517 ms | about 370 ms (estimate: 2550 x 145 us) |
| RSA-2048 signature check | 3415 ms | 1342 ms | about 120 ms (19 x 6.35 ms) |
| ECDSA P-256 check | 13235 ms | 3225 ms | |
| Handshake, 2 x X25519 + 3 RSA checks | 13.3 s | 5.0 s | |
| ChaCha20-Poly1305 / AES-128-GCM | 24 / 9 KB/s | 66 / 33 KB/s | |

Programs must run from fast RAM: ST-RAM is 16 bits wide and shared with
the video, and holds a 50 MHz 68030 back to below the speed of a stock
16 MHz one. The programs here (and EMAIL.PRG) ask TOS for fast RAM in
their header (PRGFLAGS=7 for tools/elf2tos.py).

Best split: X25519 on the 68030 (about 0.5 s each), the RSA signature
checks on the DSP (about 0.12 s each): a handshake in about 1.5 s.

## RSA in 68030 assembly (RSA030.PRG), 25 Oct 2026

A TT without a DSP took 17 s over Gmail's certificates (RSA-2048,
RSA-4096, RSA-2048) and 22 s over GMX's, and both servers hung up during
the handshake (two testers' logs). BearSSL's portable C builds each
32-bit multiply-add of a Montgomery multiplication out of 64-bit C
arithmetic, about 530 cycles each; PolarSSL has an
assembly loop for the 68020 and up there. `mont030.S` is that loop:
`mulu.l` with a 64-bit result, two of them per word, about 130 cycles.

| Emulated (Hatari) | RSA-2048 | RSA-3072 | RSA-4096 |
|---|---|---|---|
| TT, 68030 32 MHz, TT RAM (EmuTOS): BearSSL i31 | 1705 ms | 4620 ms | 8175 ms |
| the same, mont030.S | 252 ms | 562 ms | 995 ms |
| Falcon, 68030 16 MHz, ST RAM (TOS 4.04): BearSSL i31 | 3960 ms | 11015 ms | 19415 ms |
| the same, mont030.S | 453 ms | 1040 ms | 1865 ms |

The same answers as BearSSL on every one. Gmail's chain on the TT: about
11.6 s of RSA before, 1.5 s with it. Not for a 68060: its 64-bit
`mulu.l` is emulated in software (060SP), so there BearSSL's C stays.

## BearSSL's br_i31_montymul in 68030 assembly (ECBENCHC/ECBENCHA.PRG)

RSA (i31), the prime curves (ec_prime_i31), ECDSA and X25519 (i31) all
multiply with br_i31_montymul. `mont31_030.S` is its loop with `mulu.l`
(31-bit words, as BearSSL keeps them), `i31_montmul_030.c` the function
around it, in place of BearSSL's `src/int/i31_montmul.c`. Emulated TT
(68030 32 MHz, TT RAM), the same checksum of the answers from both:

| | BearSSL C | with mont31_030.S |
|---|---|---|
| RSA-2048 check, i31 | 1710 ms | 421 ms |
| RSA-4096 check, i31 | 8165 ms | 1740 ms |
| P-256 point multiply, i31 (m31: 2020 ms) | 4920 ms | 1520 ms |
| ECDSA P-256 check, i31 (m31: 3805 ms) | 9865 ms | 2970 ms |
| P-384 point multiply, i31 | 13265 ms | 3705 ms |
| ECDSA P-384 check, i31 | 26995 ms | 7625 ms |
| X25519, i31 (m31: 515 ms) | 2980 ms | 972 ms |

For RSA, rsa030.c (252 and 995 ms) is faster still: it needn't be
constant-time. For P-256 and P-384, i31 with mont31_030.S beats the m31
code; X25519 stays fastest on m31. The ECDSA checks gain least: at 8 or
12 words the C around each multiplication costs as much as it.
