# C++ Binary Instrumentation & RSA Side-Channel Analysis

A low-level C++ project exploring **dynamic binary instrumentation, instruction-level profiling, runtime control-flow analysis, and side-channel behavior** using Intel PIN.

The project contains two related components:

* **Instruction Profiler** — collects instruction, operand, register, memory, and execution-footprint statistics from x86 programs.
* **RSA Side-Channel Analysis** — instruments a vulnerable RSA implementation, extracts secret-dependent branch behavior, and reconstructs the RSA key from the resulting execution trace.

> **Primary language:** C++
> **Instrumentation:** Intel PIN
> **Analysis:** Python 3
> **Platform:** 64-bit Linux

---

## Motivation

I wanted to work below the application layer and understand what actually happens at the level of **machine instructions, control flow, memory accesses, and runtime execution**.

The project started from a simple question:

> **If a program's control flow depends on a secret, can that dependency be observed from its execution trace?**

To investigate this, I built an Intel PIN-based instrumentation tool that observes a target executable at runtime, records relevant control-flow behavior, filters unrelated execution, and analyzes the resulting trace to recover secret-dependent information.

The project also includes a lower-level instruction profiler for exploring the execution characteristics of x86 workloads.

---

# Components

## 1. Instruction Profiler

A C++ Intel PIN tool for collecting low-level execution statistics from x86 programs.

The profiler tracks:

* Instruction-type frequencies
* Loads and stores
* Direct and indirect calls
* Conditional and unconditional branches
* Vector, SSE, and MMX instructions
* Logical, shift, rotate, and flag operations
* Instruction lengths
* Operand counts
* Register reads and writes
* Memory operand distributions
* Instruction and data memory footprints
* Immediate and displacement ranges
* Estimated CPI under a simple memory-cost model

The instrumentation combines **static instruction properties**, available during instrumentation, with **runtime memory addresses**, which are only available during program execution.

---

## 2. RSA Side-Channel Analysis

The second component explores how **secret-dependent control flow can leak information through branch behavior**.

The target RSA implementation uses the classic **square-and-multiply** structure:

```text
result = 1

for each key bit:
    result = square(result)

    if key_bit == 1:
        result = multiply(result, M)

return result
```

The conditional branch depends on the secret key bit. By observing the branch outcomes during execution, it is possible to recover information about the key.

Rather than modifying the RSA implementation, the project uses **Intel PIN dynamic binary instrumentation** to observe its execution externally.

---

# Architecture

```text
                         Target RSA Binary
                                │
                                ▼
                       ┌─────────────────┐
                       │   Intel PIN     │
                       │ Dynamic Binary  │
                       │ Instrumentation │
                       └────────┬────────┘
                                │
                                │ Branch trace
                                │
                                ▼
                       ┌─────────────────┐
                       │     key.out     │
                       │                 │
                       │ address +       │
                       │ branch outcome  │
                       └────────┬────────┘
                                │
                                ▼
                       ┌─────────────────┐
                       │    analyse.py   │
                       │                 │
                       │ Trace parsing   │
                       │ Branch matching │
                       │ Key recovery    │
                       └────────┬────────┘
                                │
                                ▼
                          Recovered Key
```

The C++ component handles the **runtime instrumentation and trace collection**. The Python component performs the higher-level trace analysis and key reconstruction.

---

# The Interesting Part: Filtering Runtime Control Flow

Recording every conditional branch is straightforward.

The difficult problem is determining **which branches actually carry information about the secret**.

A real executable contains many branches unrelated to the RSA computation. Dumping every branch therefore produces a noisy trace containing a large amount of irrelevant control-flow information.

The instrumentation addresses this by filtering branch behavior during execution.

It:

1. Restricts observation to branches belonging to the **main executable**.
2. Tracks recently observed branch addresses.
3. Maintains sets of candidate **interesting** and **uninteresting** branches.
4. Uses branch history to identify recurring control-flow relationships.
5. Records the branch sequence needed for subsequent reconstruction.

This means the instrumentation performs part of the filtering **during execution**, rather than simply generating an unbounded branch log and attempting to process everything afterward.

---

# Trace Reconstruction

The generated trace contains branch addresses together with their taken/not-taken status.

The analysis identifies two characteristic branches produced by the square-and-multiply computation.

### Loop Branch

The loop branch produces a characteristic execution pattern:

```text
111111111111...110
```

The branch is repeatedly taken while the loop executes and is not taken when the loop exits.

### Secret-Dependent Branch

A second branch is interleaved with the loop branch.

Its sequence of taken/not-taken outcomes corresponds to the secret-dependent branch inside the square-and-multiply loop.

The analyzer therefore:

1. Parses the recorded branch trace.
2. Counts executions of each branch address.
3. Identifies the loop branch from its execution pattern.
4. Searches for another branch with the corresponding execution frequency.
5. Verifies that the candidate branch is interleaved with the loop.
6. Extracts its branch outcomes.
7. Converts the outcomes into key bits.
8. Reconstructs the key as an integer.

This separates the problem into two stages:

```text
Runtime execution
      │
      ▼
Branch observation
      │
      ▼
Control-flow filtering
      │
      ▼
Trace reconstruction
      │
      ▼
Secret recovery
```

---

# Result

The analysis recovers the RSA key:

```text
2753
```

The recovered value can then be passed back to the target program for verification:

```bash
./leaky_rsa -a 2753
```

Expected output:

```text
Your Key is Correct
```

---

# Repository Structure

```text
.
├── README.md
├── LICENSE
│
├── instruction-profiler/
│   ├── README.md
│   ├── profiler.cpp
│   ├── makefile
│   └── makefile.rules
│
└── rsa-side-channel/
    ├── README.md
    ├── tracer.cpp
    ├── analyse.py
    ├── key.txt
    ├── makefile
    └── makefile.rules
```

### `instruction-profiler/`

Dynamic instruction and memory-footprint analysis using Intel PIN.

### `rsa-side-channel/`

Dynamic branch instrumentation and RSA side-channel analysis.

### `tracer.cpp`

C++ PIN tool responsible for runtime branch instrumentation and trace generation.

### `analyse.py`

Python trace parser, branch identification logic, and key reconstruction.

---

# Building

## Requirements

* 64-bit Linux
* Intel PIN 4.1
* C++ compiler supported by Intel PIN
* Python 3

Set `PIN_ROOT` to the Intel PIN installation.

### Build the RSA tracer

```bash
cd rsa-side-channel
make
```

The project uses Intel PIN's standard tool build infrastructure.

---

# Running the RSA Analysis

### 1. Build the instrumentation tool

```bash
cd rsa-side-channel
make
```

### 2. Generate the branch trace

```bash
/path/to/pin \
    -t ./obj-intel64/tracer.so \
    -o key.out \
    -- /path/to/leaky_rsa
```

This produces a runtime trace containing branch addresses and their taken/not-taken status.

### 3. Analyze the trace

```bash
python3 analyse.py key.out
```

The analyzer reconstructs the key and writes it to:

```text
key.txt
```

Expected result:

```text
2753
```

### 4. Verify the recovered key

```bash
./leaky_rsa -a $(cat key.txt)
```

Expected output:

```text
Your Key is Correct
```

---

# What I Learned

The project provided hands-on experience with areas that are difficult to encounter in conventional application development:

* **C++ systems programming**
* Dynamic binary instrumentation
* x86 instruction and branch behavior
* Runtime control-flow tracing
* Basic-block and instruction-level instrumentation
* Memory and register analysis
* Execution-trace processing
* Side-channel analysis
* Runtime versus static program information

More importantly, it made the connection between **source-level code and the machine-level behavior produced by that code** much more concrete.

---

# Engineering Takeaways

The biggest lesson was that instrumentation is fundamentally a **data-selection problem**.

Collecting more execution data does not necessarily produce a better analysis. Instrumenting every branch creates a large and noisy trace; the useful information comes from identifying the smallest set of observations that preserves the behavior being studied.

The resulting workflow became:

```text
instrument → filter → identify → reconstruct
```

rather than simply:

```text
instrument → dump everything → analyze later
```

This was also a useful exercise in designing low-level tooling where **runtime overhead, trace volume, and signal-to-noise ratio** directly affect the usefulness of the system.

---

## Context

This project originated as work for **CS422** and has been organized here as a standalone systems project.

The implementation intentionally stays close to the underlying **Intel PIN APIs**, exposing the mechanics of instruction-level instrumentation and runtime execution analysis rather than hiding them behind higher-level abstractions.
