// ============================================================================
//  reves_puzzle.cpp — Benchmark & Verification Harness for the Frame-Stewart
//                     Solver (solver.h)
// ============================================================================
//
//  Generates the optimal move sequence, prints diagnostics, then verifies
//  correctness by locally simulating every move against peg constraints.
//
//  Build:  g++ -std=c++17 -O2 -o reves_puzzle reves_puzzle.cpp
// ============================================================================

#include "solver.h"

#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <vector>

// ---------------------------------------------------------------------------
//  Diagnostic output (I/O decoupled from Solver — Single Responsibility)
// ---------------------------------------------------------------------------

/// Prints the complete DP table: disk count, split k, and optimal move count.
static void printDPTable(const Solver& solver) {
    std::cout << "+-------+---------+-----------+\n"
              << "| Disks | Split k |  FS moves |\n"
              << "+-------+---------+-----------+\n";
    for (int i = 0; i <= solver.maxDisks(); ++i) {
        std::cout << "|  " << std::setw(3) << i
                  << "  |   " << std::setw(3) << solver.optimalSplit(i)
                  << "   | " << std::setw(9) << solver.optimalMoves(i) << " |\n";
    }
    std::cout << "+-------+---------+-----------+\n";
}

/// Prints the generated move sequence with peg labels.
static void printMoves(const Solver& solver) {
    const auto& moves = solver.moves();
    for (size_t i = 0; i < moves.size(); ++i) {
        const auto& m = moves[i];
        std::cout << "  Move " << std::setw(3) << (i + 1)
                  << ":  Disk " << m.disk
                  << "  " << Solver::kPegLabel[m.from_peg]
                  << " -> " << Solver::kPegLabel[m.to_peg] << "\n";
    }
}

// ---------------------------------------------------------------------------
//  Correctness verification — simulates every move on local peg state
// ---------------------------------------------------------------------------

/// Replays move sequence against four pegs, asserting all constraints hold.
static void verifyMoves(const Solver& solver, int n) {
    const auto& moves = solver.moves();

    std::vector<std::vector<int>> pegs(Solver::kNumPegs);
    for (int d = n; d >= 1; --d)
        pegs[0].push_back(d);

    for (const auto& m : moves) {
        assert(!pegs[m.from_peg].empty() &&
               pegs[m.from_peg].back() == m.disk &&
               "Invalid Pop: disk constraint violated");
        pegs[m.from_peg].pop_back();

        assert((pegs[m.to_peg].empty() || pegs[m.to_peg].back() > m.disk) &&
               "Invalid Push: disk constraint violated");
        pegs[m.to_peg].push_back(m.disk);
    }

    for (int p = 0; p < Solver::kNumPegs - 1; ++p)
        assert(pegs[p].empty() && "Auxiliary peg not empty at terminus");
    assert(static_cast<int>(pegs[Solver::kNumPegs - 1].size()) == n &&
           "Destination peg missing disks");
}

// ---------------------------------------------------------------------------
//  Entry point
// ---------------------------------------------------------------------------

int main() {
    int N;
    std::cout << "Enter the number of disks: ";
    if (!(std::cin >> N) || N < 1) {
        std::cerr << "Invalid input. Please enter a positive integer.\n";
        return 1;
    }

    Solver solver(N);

    // -- Display DP table ---------------------------------------------------
    std::cout << "\n=== Frame-Stewart DP Table (n = 0.." << N << ") ===\n\n";
    printDPTable(solver);

    // -- Solve and display move sequence ------------------------------------
    int total_moves = solver.solve(N);

    std::cout << "\n=== Move Sequence for " << N << " Disks ===\n\n";
    printMoves(solver);

    std::cout << "\n  Total moves: " << total_moves
              << "  (expected: " << solver.optimalMoves(N) << ")\n";

    // -- Integrity verification ---------------------------------------------
    assert(total_moves == solver.optimalMoves(N) &&
           "FAILED: Move count does not match DP optimal.");

    verifyMoves(solver, N);

    std::cout << "\n  [PASS] n=" << N << " solved in " << total_moves << " moves (optimal).\n"
              << "  [PASS] All " << N << " disks are exclusively on peg "
              << Solver::kPegLabel[Solver::kNumPegs - 1] << ".\n\n";

#ifdef _WIN32
    std::cout << "Finished! ";
    system("pause");
#else
    std::cout << "Press Enter to exit...";
    std::cin.ignore(10000, '\n');
    std::cin.get();
#endif

    return 0;
}
