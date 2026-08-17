# Official External Baselines

The existing `benchmark_runner` is an **in-repository development baseline**.
Its `json`, `protobuf`, and `messagepack` labels are local implementations and
must never be presented as results from official third-party libraries.

Official-library work is a separate benchmark suite, not an extension of the
current CSVs. Before it is enabled, the study manifest must identify:

- the exact library and version (for example, a named JSON library, Google
  Protocol Buffers C++, and MessagePack C++);
- generated schema/source inputs and all compiler options;
- allocator or arena configuration;
- the workload and whether the measurement is stateless or stateful; and
- a distinct `benchmark_classification` such as `official_external_baselines`.

Do not add official-library rows to an `in-repository development baselines`
study, and do not use those local rows to make claims about external libraries.
The external adapters belong in their own CMake targets and manifest-backed
studies once their dependencies are deliberately selected and pinned.
