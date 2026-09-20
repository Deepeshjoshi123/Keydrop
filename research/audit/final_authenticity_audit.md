# Final Keydrop IEEE Authenticity Audit

**Audit date:** 2026-08-30  
**Manuscript audited:** the LaTeX source supplied in the user attachment (not present as a writable repository file).  
**Evidence boundary:** repository source and tests; study `phase7-official-20260818`; its manifest, raw CSVs, processed CSVs, and generated graphs. Historical files under `research/benchmark/raw_data/` and the top-level `processed/` directory are explicitly labelled legacy and are not treated as authoritative.

## Executive result

The source implementation supports the central architectural description: schema registration, binary schema-directed encode/decode, validation, optional runtime optimization, packet resynchronization, and TCP/UDP/WebSocket transport adapters exist. It **does not** support the paper's claim that the stream mechanisms are only architectural extension points: adaptive dictionary, delta packets, batching, keyframes, and recovery code are implemented and unit tested, although the paper's reported stateless-format benchmark does not evaluate them.

The paper's numerical result set is **not submission-ready**. Its 17/56/18/47 B, latency, and throughput values match the manifest-backed study's `raw/format_trials.csv`, but the manuscript says 100,000 iterations while the manifest and every raw row say 50,000. The manuscript's allocation figure and Table III conflict with that study's raw data. Their provenance is historical/unknown. Do not silently replace them: rerun a clean study or establish the final figure provenance.

## Claim-audit matrix

| ID | Paper section | Claim (abridged) | Type | Repository evidence | Test/benchmark evidence | Status | Required action |
|---|---|---|---|---|---|---|---|
| C01 | Abstract, Intro | C++17 schema-aware runtime | Architecture | `SchemaDef`, registry and `SchemaRuntime` in `include/keydrop/schema/` and `src/schema/schema_runtime.cpp`; CMake sets C++17 | schema/registry/runtime tests registered | CONFIRMED | None |
| C02 | Abstract, Intro | Field schemas are fully resolved once at registration | Implementation | Registration validates and caches `PacketLayout`; `send` still maps named fields and resolves/caches schema at encode time | schema runtime tests | PARTIALLY CONFIRMED | Say that layout metadata is cached at registration; named-payload mapping and payload validation remain per message. |
| C03 | Abstract, Intro | Messages omit field metadata | Implementation | `encode_ordered_with_schema` writes two-byte `message_id` then ordered values | `format_trials.csv`: 17 B Keydrop fixed payload | CONFIRMED | Scope to registered schema and evaluated payload. |
| C04 | Architecture | Packet construction is transport-independent; packet can be sent without re-encoding | Transport | `Buffer` is produced by `SchemaRuntime`; abstract `Transport::send(const Buffer&)` consumes it | TCP/UDP/WebSocket tests exist | CONFIRMED | Distinguish code/test support from network-performance evaluation. |
| C05 | Architecture | Binary encoding/packet builder/reader are implemented | Implementation | `Encoder`, `PacketBuilder`, `PacketReader` in `src/core/` | encoder, builder, reader tests | CONFIRMED | None |
| C06 | Architecture | Runtime validation rejects malformed data | Reliability | `SchemaValidator`; bounds checks and `CorruptionDetector::check_keydrop_packet` called by `receive_with_schema` | schema validator/runtime/corruption tests | CONFIRMED | Use “validation” rather than general reliability guarantee. |
| C07 | Architecture | CRC/integrity is applied to every packet | Reliability | CRC is optional in `CorruptionDetector`; `SchemaRuntime` wire encoder does not append a CRC envelope | corruption tests exercise detector options | CONTRADICTED if stated universally | Describe CRC as an available detector option, not the default packet format. |
| C08 | Architecture, limitations | Corruption recovery is only an extension point/future work | Reliability | `PacketSynchronizer` and `SchemaRuntime::receive_recovered_stream` are implemented | `test_packet_synchronizer.cpp`, `test_schema_runtime.cpp` test skipped-byte recovery | CONTRADICTED | State that stream resynchronization exists and is unit tested, but is not evaluated in the reported benchmark or under network loss. |
| C09 | Architecture | TCP transport exists | Transport | `TcpAdapter` native socket send/receive with length framing | TCP tests, subject to local networking | CONFIRMED | No transport performance claim. |
| C10 | Architecture | UDP transport exists | Transport | `UdpAdapter` is compiled in `src/transport/udp_adapter.cpp` | `test_udp_adapter` registered (can skip) | CONFIRMED | Mention UDP explicitly or avoid an exhaustive TCP/WebSocket list. |
| C11 | Architecture | WebSocket is merely a TCP wrapper / not full protocol | Transport | `WebSocketAdapter` implements RFC 6455 opening handshake, frames, masking, fragmentation, ping/pong/close | handshake and adapter tests registered | CONTRADICTED if called wrapper-only | Describe implemented RFC-6455 subset/features; do not claim interoperability testing beyond repository tests. |
| C12 | Architecture | Linux and Windows support | Portability | `_WIN32` and POSIX branches in socket/TCP code; `ws2_32` linkage | no attached Windows test artifact | PARTIALLY CONFIRMED | “Code targets Linux and Windows; experimental results are Linux-only.” |
| C13 | Architecture | Stateless benchmark path has pass-through runtime optimization | Algorithm | `RuntimeOptimizer` is called only when enabled; benchmark `format_benchmark` config must be named | manifest format benchmark is stateless but does not establish optimizer configuration in the paper | PARTIALLY CONFIRMED | State exact configuration or omit pass-through assertion. |
| C14 | Intro, limitations | Dictionary, delta, batching, keyframes are extension points only | Algorithm | `AdaptiveDictionary`, `StreamOptimizer`, `send_stream`, delta packets and keyframe config are implemented | dictionary/stream/delta/adaptive tests and separate benchmarks present | CONTRADICTED | Classify as implemented and tested, not evaluated by Table III. |
| C15 | Related work | Keydrop has no zero-copy decode | Implementation | `BufferView` exists, but `PacketReader::read_string/read_bytes` return owning values and schema decoding materializes `FieldValue` | buffer/reader tests | CONFIRMED | Retain this wording. |
| C16 | Architecture | Packet equation broadly equals header plus fixed field widths | Mathematical | Message ID is 2 B; fixed values are fixed-width; string/bytes use `u16` length prefix and variable data; optional optimizer changes format | packet layout/encoder tests | PARTIALLY CONFIRMED | Restrict equation to unoptimized fixed-width payloads or add variable-length and optional-optimization terms. |
| C17 | Methodology | `-O3 -DNDEBUG` Release build | Methodology | Manifest records `Release`; it does not record flags; CMake itself adds no `-O3`/`-DNDEBUG` | manifest only | NOT VERIFIABLE FROM REPOSITORY | Remove exact flags unless captured in the build command/cache. |
| C18 | Methodology | 100,000 iterations, 30 trials | Experimental result | `manifest.json` and all `raw/format_trials.csv` rows state 50,000; 30 trials | 30 raw trials | CONTRADICTED | Change to 50,000 iterations × 30 trials, or rerun a manifest-backed 100,000-iteration study. |
| C19 | Results | JSON, protobuf-like, MessagePack-like, Keydrop values use same harness/payload | Benchmark methodology | `format_benchmark.cpp`; raw format CSV has all four rows per trial | format raw CSV, 30 trials | CONFIRMED | Retain the in-repository baseline identity. |
| C20 | Results | 17/56/18/47 B packet sizes | Experimental result | `format_summary.csv`: Keydrop 17, JSON 56, protobuf 18, messagepack 47 | raw format trials, 30 samples each | CONFIRMED | Identify this as the 50,000-iteration format benchmark. |
| C21 | Results | 210/325/118/122 ns encode means | Experimental result | `format_summary.csv` values 209.701/324.902/117.906/121.923 | raw format trials | CONFIRMED | Round consistently; retain provenance. |
| C22 | Results | 165/220/31.4/35.1 ns decode means | Experimental result | `format_summary.csv` values 165.019/220.228/31.382/35.059 | raw format trials | CONFIRMED | Round consistently; retain provenance. |
| C23 | Results | 2.68/1.85/6.75/6.44 M ops/s | Experimental result | `format_summary.csv` gives 2.682/1.850/6.746/6.440 M | raw format trials | CONFIRMED | “Higher than JSON; lower than both in-repository binary baselines.” |
| C24 | Abstract, results | Keydrop is smallest among the four evaluated formats | Experimental result | C20 | raw format trials | CONFIRMED | Say “among the four in-repository-format benchmark implementations.” |
| C25 | Abstract, results | Keydrop is smallest against current official baselines | Experimental result | `official_summary.csv`: Keydrop stateless 25.33 mean vs protobuf 25.5 and MsgPack 23 across mixed workloads | official study differs in formats/workloads | UNSUPPORTED | Do not conflate the two studies. |
| C26 | Results, Table III, Fig. 10 | Allocation count ≈3/7/6/5 and 626/67/141/344 B/enc | Experimental result | manifest raw CSV gives JSON 1.5/313, Keydrop 3/193.5, protobuf 3.5/33.5, MsgPack 3/70.5 per encode | supplied `memory_behavior.png` shows the paper numbers but has no study provenance | CONTRADICTED | **RESULT PROVENANCE CONFLICT — MANUAL VERIFICATION REQUIRED.** Rerun or prove figure source; do not alter selectively. |
| C27 | Methodology | Allocation count means logical output-buffer allocations | Methodology | `HeapTracker` globally intercepts `new`/`new[]` in its thread-local tracking window | code | CONTRADICTED | Call these gross heap-allocation events and requested bytes during the encode window; not logical output-buffer allocations or memory usage. |
| C28 | Results discussion | Keydrop is “never first, never last” across every processing metric | Experimental result | Keydrop is first for packet size; allocation raw data gives Keydrop neither an invariant middle ranking; wording includes non-processing metric ambiguity | C20/C26 | CONTRADICTED | Replace with: “For combined throughput, Keydrop exceeded JSON but remained below both in-repository binary baselines.” |
| C29 | Limitations | Stateful mechanisms are not benchmarked by reported numbers | Benchmark methodology | Table III source is `format_benchmark`; stateful study is separate | manifest raw format/stream CSVs | CONFIRMED | Amend wording to “implemented and separately benchmarked, but not part of Table III.” |
| C30 | Reproducibility | Open-source implementation/reproducible study | Reproducibility | scripts, raw/processed CSV, graphs, and manifest exist | manifest is `dirty: true`, `publication_eligible: false`; no test log exists at stated path | PARTIALLY CONFIRMED | Do not call the study publication-eligible; rerun on a clean commit with retained CTest log. |
| C31 | Tests/fuzzing | Sanitizer-verified fuzzing or memory safety | Reliability | only `test_fuzz_reliability.cpp` is a deterministic random-input test; no ASan/UBSan/fuzzer configuration found | test target exists | UNSUPPORTED | Do not claim sanitizers or coverage-guided fuzzing. |

## Result provenance and Table III reconciliation

The manifest-backed study is the only complete evidence package. It is nevertheless marked `publication_eligible: false` because it was collected from a dirty tree. Its source commit is `22edd19e4b53b833da7ad2150c7d19e4d567630d` and its environment is Linux, Ryzen 5 7520U, 8 logical CPUs, 16,000,663,552 B memory, GCC 13.3.0, CMake 3.28.3, Release, 50,000 iterations, 30 trials.

| Metric | Manuscript / supplied figure | Manifest-backed format study | Disposition |
|---|---:|---:|---|
| Packet bytes (JSON/Protobuf/MsgPack/Keydrop) | 56/18/47/17 | 56/18/47/17 | Verified |
| Encode ns | 325/118/122/210 | 324.902/117.906/121.923/209.701 | Verified after stated rounding |
| Decode ns | 220/31.4/35.1/165 | 220.228/31.382/35.059/165.019 | Verified after stated rounding |
| Throughput M/s | 1.85/6.75/6.44/2.68 | 1.850/6.746/6.440/2.682 | Verified after stated rounding |
| Allocation events/encode | ≈3/≈7/≈6/≈5 | 1.5/3.5/3/3 | Conflict |
| Requested bytes/encode | 626/67/141/344 | 313/33.5/70.5/193.5 | Conflict |

The table and Fig. 10 therefore cannot remain as a single internally consistent experimental result set. The apparent figure source is an unproven historical run. No numerical edit should be made until the author either identifies the complete raw CSV/command for it or regenerates all figures and Table III from a clean immutable study.

## Required manuscript edits (safe wording)

1. Replace every `100,000 iterations × 30 trials` statement and Table II value with `50,000 iterations × 30 trials`, **only if** this paper is anchored to `phase7-official-20260818`; otherwise rerun the named 100,000-iteration study.
2. Replace “resolves field identifiers, types, and ordering at registration time” with: “validates schemas and caches a packet layout at registration; named-payload mapping and payload validation remain on the encode path.”
3. Replace “stateful mechanisms exist as architectural extension points” with: “dictionary, stream, delta, batching, keyframe, and resynchronization mechanisms are implemented and unit tested, but are outside the reported stateless format benchmark.”
4. Replace the V-D sentence with: “For combined throughput, Keydrop exceeded JSON (2.68 versus 1.85 M ops/s) but remained below the Protocol Buffers-like and MessagePack-like baselines (6.75 and 6.44 M ops/s).”
5. Replace allocation terminology everywhere with: “gross heap-allocation events and requested bytes observed by thread-local encode-window instrumentation.”
6. Remove `-O3 -DNDEBUG` from Table II/methodology unless a build cache or compiler command record establishes those exact flags.
7. Do not state that validation or CRC provides packet-loss recovery. The implementation has a schema-guided stream resynchronizer, but no reported loss/reordering or network recovery experiment.
8. Keep “in-repository Protocol Buffers-like/MessagePack-like baseline” throughout. The repository now also contains a distinct official-external-baseline study, which must not be mixed with these results.
9. Resolve all `\includegraphics` paths before compilation. The supplied LaTeX names `fig5.png`, `encoding_benchmark.png`, `decoding_benchmark.png`, and `Throughput_png.png` are not present in the repository. Available generated graph names differ and must only be substituted after provenance is confirmed.

## Tests and build audit

| Item | Result | Evidence |
|---|---|---|
| Local CMake configure/test run | 35/35 PASS | Configured with MinGW Makefiles and GNU 6.3.0 on Windows; `ctest --output-on-failure --timeout 60` completed successfully. |
| Full local build | FAIL (integration example only) | `integration/04-tcp-client.cpp:36` uses `std::this_thread::sleep_for` without a declaration of `std::this_thread` (missing `<thread>` include). All CTest executables had already built. |
| Existing study CTest result | NOT VERIFIED | Manifest points to `test/ctest.log`, but that file is absent. |
| Sanitizer run | NOT AVAILABLE | No sanitizer configuration or artifact found. |
| Deterministic fuzz-like test | PRESENT, NOT EXECUTED | `tests/test_fuzz_reliability.cpp`. |

## Feature classification

**Implemented and tested:** schemas/registry/layout; binary encoder/reader; payload validation; packet builder; optional zero-value omission; corruption checks; schema-guided stream resynchronization; TCP, UDP and WebSocket adapters; adaptive dictionary; stream batching, packet reuse, delta packets and keyframes; buffer/payload pools.

**Implemented but not established by Table III:** all stateful features, recovery, transport paths, and fast codec path. Separate benchmarks exist for several but no result from them belongs in the paper unless explicitly traced.

**Not implemented as claimed:** end-to-end schema decode zero-copy; universal CRC envelope; loss/reordering recovery evaluation; sanitizer verification; Windows experimental evaluation.

## IEEE presentation and submission blockers

1. **Blocking:** allocation figure/Table III conflict with the only manifest-backed raw dataset.
2. **Blocking:** methods claim 100,000 iterations but the traceable result study is 50,000.
3. **Blocking:** the traceable study is dirty and explicitly `publication_eligible: false`; CTest log is absent.
4. **Blocking for local PDF validation:** supplied LaTeX is an attachment, not a repository `.tex` file, and its four named figure files are absent. It cannot be compiled or visually inspected here.
5. **Required language correction:** stateful mechanisms and recovery are implemented/tested but unevaluated by Table III; they are not merely extension points.
6. **Required language correction:** field layout caching at registration is not equivalent to resolving all schema work once per message stream.

## Audit totals and scores

- Claims audited: 31 grouped substantive claims
- Confirmed: 15
- Partially confirmed: 4
- Unsupported/not verifiable: 3
- Contradicted: 8
- Experimental result groups verified: 4 (size, encode latency, decode latency, throughput)
- Experimental result groups not verified/conflicted: 2 (allocation count, requested bytes)
- Tests passed: 35/35 local CTest targets
- Tests failed: 0 CTest targets
- Build failures: 1 integration example target (`04-tcp-client`); existing Linux-study CTest artifact remains unavailable
- Authenticity score: **5.5/10** (architecture is substantially real; study metadata/result provenance prevents a trustworthy results section)
- Technical paper score: **5/10** pending the blocking corrections and reproducible clean-study rerun

## Files modified by this audit

- `research/audit/final_authenticity_audit.md` — this evidence report only.

No manuscript source was modified: the supplied LaTeX exists only as an external attachment and no writable `.tex` source is in the repository. No benchmark measurement or graph value was changed.
