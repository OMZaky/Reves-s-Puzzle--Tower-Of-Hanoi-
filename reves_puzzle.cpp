// ============================================================================
//  reves_puzzle.cpp — Base benchmark for the Frame-Stewart Solver
// ============================================================================

#include "solver.h"

#include <iostream>
#include <vector>
#include <cassert>
#include <cstdlib>

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
    solver.printDPTable();

    // -- Solve and sequence evaluation --------------------------------------
    int total_moves = solver.solve(N);

    std::cout << "\n=== Move Sequence for " << N << " Disks ===\n\n";
    solver.printMoves();

    std::cout << "\n  Total moves: " << total_moves
              << "  (expected: " << solver.optimalMoves(N) << ")\n";

    // -- Logic Integrity Verification ---------------------------------------
    assert(total_moves == solver.optimalMoves(N) && "FAILED: Move count does not match DP optimal.");

    // Simulate the moves locally to ensure strict correctness
    std::vector<std::vector<int>> pegs(Solver::kNumPegs);
    for (int d = N; d >= 1; --d) {
        pegs[0].push_back(d);
    }

    for (const auto& m : solver.moves()) {
        assert(!pegs[m.from_peg].empty() && pegs[m.from_peg].back() == m.disk && "Invalid Pop: Disk constraint failed");
        pegs[m.from_peg].pop_back();

        assert((pegs[m.to_peg].empty() || pegs[m.to_peg].back() > m.disk) && "Invalid Push: Disk constraint failed");
        pegs[m.to_peg].push_back(m.disk);
    }

    for (int p = 0; p < Solver::kNumPegs - 1; ++p) {
        assert(pegs[p].empty() && "Auxiliary pegs not empty at terminus");
    }
    assert(static_cast<int>(pegs[3].size()) == N && "Destination peg missing disks");

    std::cout << "\n  [PASS] n=" << N << " solved in " << total_moves << " moves (optimal).\n"
              << "  [PASS] All " << N << " disks are exclusively on peg " << Solver::kPegLabel[3] << ".\n\n";

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
