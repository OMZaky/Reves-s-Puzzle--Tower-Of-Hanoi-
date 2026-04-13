<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/Algorithm-Frame--Stewart-orange?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Platform-Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" />
</p>

<h1 align="center">🗼 Reve's Puzzle</h1>
<h3 align="center">4-Peg Tower of Hanoi — Optimal Solver & Interactive Terminal Visualizer</h3>

<p align="center">
  A high-performance C++17 implementation of the <strong>Frame-Stewart algorithm</strong> for the 4-peg Tower of Hanoi problem, <br/>
  paired with a fully interactive, color-coded ASCII terminal visualizer with timeline controls.
</p>

---

## 📖 What is Reve's Puzzle?

**Reve's Puzzle** is a generalization of the classic Tower of Hanoi that uses **4 pegs** instead of 3. It was first proposed by British puzzlemaker Henry Dudeney in 1907.

The challenge: move `n` disks from the first peg to the last, obeying two rules:

1. Only one disk may be moved at a time
2. A larger disk may never be placed on top of a smaller one

With 3 pegs the solution always requires `2ⁿ − 1` moves. The 4th peg allows dramatically fewer moves — but finding the **optimal** solution is far more complex.

> **Example:** 8 disks require **255 moves** with 3 pegs, but only **33 moves** with 4 pegs!

---

## 🧮 The Frame-Stewart Algorithm

The **Frame-Stewart algorithm** (1941) provides the conjectured-optimal solution using dynamic programming and a divide-and-conquer strategy.

### Core Recurrence

For `n` disks and `4` pegs, the optimal move count `FS(n)` is:

```
FS(n) = min over k ∈ [1, n-1] of { 2·FS(k) + 2^(n-k) − 1 }
```

Where `k` is the **split point** — the number of disks temporarily moved to an auxiliary peg using all 4 pegs, while the remaining `n − k` disks are solved using classic 3-peg Hanoi.

### Strategy

```
1. Move the top k disks from Source → Auxiliary  (using 4 pegs, Frame-Stewart)
2. Move remaining n-k disks from Source → Target  (using 3 pegs, classic Hanoi)
3. Move the k disks from Auxiliary → Target        (using 4 pegs, Frame-Stewart)
```

### Optimal Splits & Move Counts

| Disks | Split k | Moves (4-peg) | Moves (3-peg) | Savings |
|:-----:|:-------:|:-------------:|:--------------:|:-------:|
| 1     | 0       | 1             | 1              | 0%      |
| 2     | 1       | 3             | 3              | 0%      |
| 3     | 1       | 5             | 7              | 29%     |
| 4     | 2       | 9             | 15             | 40%     |
| 5     | 2       | 13            | 31             | 58%     |
| 6     | 3       | 17            | 63             | 73%     |
| 8     | 3       | 33            | 255            | 87%     |
| 10    | 4       | 49            | 1,023          | 95%     |
| 15    | 5       | 129           | 32,767         | 99.6%   |
| 20    | 6       | 289           | 1,048,575      | 99.97%  |

---

## 🏗️ Architecture

The project is cleanly separated into three files with a decoupled design:

```
Reve's Puzzle/
├── solver.h            # Frame-Stewart algorithm engine (header-only)
├── reves_puzzle.cpp    # Benchmark driver with correctness verification
├── visualizer.cpp      # Interactive terminal visualizer with timeline controls
├── .gitignore
└── README.md
```

### `solver.h` — Algorithm Engine

A **header-only**, self-contained solver class:

- **DP precomputation** — `O(n²)` time, `O(n)` space to fill the optimal split table
- **Recursive move generation** — Frame-Stewart for 4-peg phases, classic Hanoi for 3-peg phases
- **Zero I/O coupling** — outputs a `std::vector<Move>` of `{disk, from_peg, to_peg}` structs
- **Pre-allocated memory** — `moves_.reserve()` eliminates dynamic allocation during recursion

### `reves_puzzle.cpp` — Benchmark & Verifier

A correctness-proving harness that:

- Displays the full DP table for all `n` values
- Generates and prints the complete move sequence
- **Simulates every move** to verify no constraint violations (disk ordering, source/target validity)
- Asserts the final state has all disks on the destination peg

### `visualizer.cpp` — Terminal Visualizer

A real-time ASCII animation engine featuring:

- 🎨 **Color-coded disks** with 12-color palette using ANSI escape sequences
- ⏯️ **Interactive timeline** — play, pause, step forward, step backward
- 📊 **Progress bar** with move counter and peg highlighting
- 🖥️ **Terminal size detection** with automatic resize waiting
- ⚡ **Async game loop** with 10ms polling and configurable frame delay

---

## 🚀 Getting Started

### Prerequisites

- **C++17 compiler** (GCC 8+, Clang 7+, or MSVC 19.14+)
- **Windows Terminal** recommended for best ANSI color support

> On Windows, [MSYS2](https://www.msys2.org/) with the UCRT64 toolchain is recommended.

### Build

```bash
# Compile the benchmark
g++ -std=c++17 -O2 -o reves_puzzle reves_puzzle.cpp

# Compile the visualizer
g++ -std=c++17 -O2 -o visualizer visualizer.cpp
```

### Run

```bash
# Interactive benchmark — prompts for disk count
./reves_puzzle

# Visualizer with defaults (prompts for disks, 400ms delay)
./visualizer

# Visualizer with CLI args: 10 disks, 200ms delay
./visualizer 10 200
```

---

## 🎮 Visualizer Controls

| Key | Action |
|:---:|--------|
| `Enter` | ⏯️ Toggle play / pause |
| `→` | ⏭️ Step forward one move |
| `←` | ⏮️ Step backward one move |
| `Esc` / `Q` | ❌ Quit |

> The visualizer starts **paused** — press `Enter` to begin the animation, or use `→` to step through manually.

---

## 🔬 Verification

The benchmark (`reves_puzzle.cpp`) performs three levels of correctness checking:

1. **Move count assertion** — generated moves must equal the DP-computed optimal count
2. **Constraint simulation** — every move is replayed on a local state to enforce:
   - The source peg is non-empty and the correct disk is on top
   - The destination peg's top disk is larger than the incoming disk
3. **Terminal state check** — all disks must reside exclusively on the target peg

```
  [PASS] n=8 solved in 33 moves (optimal).
  [PASS] All 8 disks are exclusively on peg D.
```

---

## 📝 License

This project is open source and available under the [MIT License](LICENSE).

---

<p align="center">
  <sub>Built with ❤️ and C++17</sub>
</p>
