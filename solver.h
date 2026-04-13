// ============================================================================
//  solver.h — Frame-Stewart Algorithm for the 4-Peg Tower of Hanoi
// ============================================================================
//
//  Header-only, I/O-free solver.  Precomputes the Frame-Stewart DP table
//  in O(n²) time / O(n) space, then generates the exact optimal move
//  sequence via recursive divide-and-conquer.
//
// ============================================================================
#ifndef SOLVER_H
#define SOLVER_H

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

/// Represents a single disk transfer between two pegs.
struct Move {
    int disk;       ///< 1-indexed disk identifier (1 = smallest).
    int from_peg;   ///< Source peg index [0, 3].
    int to_peg;     ///< Destination peg index [0, 3].
};

/// Generates optimal move sequences for Reve's Puzzle via Frame-Stewart DP.
///
/// Usage:
///   Solver solver(n);
///   solver.solve(n);
///   for (const auto& m : solver.moves()) { /* ... */ }
class Solver {
public:
    static constexpr int  kNumPegs    = 4;
    static constexpr char kPegLabel[] = {'A', 'B', 'C', 'D'};

    /// Constructs the solver and precomputes the DP table for [0, max_disks].
    explicit Solver(int max_disks)
        : max_disks_(max_disks), dp_table_(max_disks + 1)
    {
        precomputeDP();
    }

    // -- Accessors -----------------------------------------------------------

    /// Returns the DP-optimal move count for @p n disks.
    [[nodiscard]] int64_t optimalMoves(int n) const noexcept { return dp_table_[n].moves; }

    /// Returns the optimal split point k for @p n disks.
    [[nodiscard]] int optimalSplit(int n)     const noexcept { return dp_table_[n].k_split; }

    /// Returns the generated move sequence (populated after solve()).
    [[nodiscard]] const std::vector<Move>& moves() const noexcept { return moves_; }

    /// Returns the maximum disk count this solver was constructed for.
    [[nodiscard]] int maxDisks() const noexcept { return max_disks_; }

    /// Generates the optimal move sequence for @p n disks.
    /// @return Total number of moves produced.
    [[nodiscard]] int solve(int n) {
        assert(n >= 0 && n <= max_disks_);

        // Pre-allocate exact capacity to eliminate reallocation during recursion
        moves_.clear();
        moves_.reserve(static_cast<size_t>(dp_table_[n].moves));

        frameStewart(n, 0, 3, 1, 2, 0);
        return static_cast<int>(moves_.size());
    }

private:
    /// DP table entry: optimal move count and its corresponding split point.
    struct DPEntry {
        int64_t moves   = 0;
        int     k_split = 0;
    };

    int                   max_disks_;
    std::vector<DPEntry>  dp_table_;
    std::vector<Move>     moves_;

    /// Fills the DP table using the Frame-Stewart recurrence:
    ///   FS(n) = min_{1 <= k <= n-1} { 2*FS(k) + 2^(n-k) - 1 }
    /// Complexity: O(n²) time, O(n) space.
    void precomputeDP() {
        dp_table_[0] = {0, 0};
        if (max_disks_ >= 1) dp_table_[1] = {1, 0};

        for (int n = 2; n <= max_disks_; ++n) {
            int64_t best   = std::numeric_limits<int64_t>::max();
            int     best_k = 1;

            for (int k = 1; k <= n - 1; ++k) {

                // Prevent 64-bit overflow. Any split leaving 62+ disks for 3 pegs 
                // is mathematically terrible anyway, so we just skip testing it.
                if (n - k >= 62) continue;

                
                int64_t cost = 2LL * dp_table_[k].moves + (1LL << (n - k)) - 1;
                if (cost < best) {
                    best   = cost;
                    best_k = k;
                }
            }
            dp_table_[n] = {best, best_k};
        }
    }

    /// Appends a move record to the pre-allocated sequence buffer.
    void recordMove(int disk, int from_peg, int to_peg) {
        moves_.emplace_back(Move{disk, from_peg, to_peg});
    }

    /// Classic 3-peg Tower of Hanoi recursion.
    /// @param offset  Disk numbering offset for the current sub-problem.
    void hanoi3(int n, int src, int tgt, int aux, int offset) {
        if (n <= 0) return;
        hanoi3(n - 1, src, aux, tgt, offset);
        recordMove(offset + n, src, tgt);
        hanoi3(n - 1, aux, tgt, src, offset);
    }

    /// Recursive 4-peg Frame-Stewart move generator.
    /// Strategy: move top-k via 4-peg, remaining n-k via 3-peg, then top-k back.
    void frameStewart(int n, int src, int tgt, int aux1, int aux2, int offset) {
        if (n <= 0) return;
        if (n == 1) { recordMove(offset + 1, src, tgt); return; }

        int k = dp_table_[n].k_split;
        frameStewart(k, src, aux1, aux2, tgt, offset);
        hanoi3(n - k, src, tgt, aux2, offset + k);
        frameStewart(k, aux1, tgt, src, aux2, offset);
    }
};

#endif // SOLVER_H
