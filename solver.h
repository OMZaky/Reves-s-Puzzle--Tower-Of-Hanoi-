// ============================================================================
//  solver.h — Frame-Stewart Algorithm for the 4-Peg Tower of Hanoi
// ============================================================================
#ifndef SOLVER_H
#define SOLVER_H

#include <vector>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <limits>

/**
 * @struct Move
 * @brief Represents a single disk transfer operation.
 * Completely decoupled from I/O and spatial peg representations.
 */
struct Move {
    int disk;       ///< 1-indexed disk number (1 = smallest)
    int from_peg;   ///< Source peg index (0-3)
    int to_peg;     ///< Target peg index (0-3)
};

/**
 * @class Solver
 * @brief Generates optimal move sequences for Reve's Puzzle using dynamic programming.
 */
class Solver {
public:
    static constexpr char kPegLabel[] = {'A', 'B', 'C', 'D'};
    static constexpr int  kNumPegs    = 4;

    /**
     * @brief Constructs the Solver and precomputes the Frame-Stewart DP table.
     * @param max_disks Maximum number of disks the solver is expected to handle.
     */
    explicit Solver(int max_disks) : max_disks_(max_disks), dp_table_(max_disks + 1) {
        computeDP();
    }

    // -- Accessors -----------------------------------------------------------
    [[nodiscard]] int64_t optimalMoves(int n) const noexcept { return dp_table_[n].moves; }
    [[nodiscard]] int optimalSplit(int n) const noexcept { return dp_table_[n].k_split; }
    [[nodiscard]] const std::vector<Move>& moves() const noexcept { return moves_; }

    /**
     * @brief Executes the Frame-Stewart algorithm to generate the exact move sequence.
     * @param n The total number of disks to solve for.
     * @return Total number of logical moves performed.
     */
    int solve(int n) {
        assert(n >= 0 && n <= max_disks_);
        
        // Eliminate dynamic memory allocation overhead during recursive execution
        moves_.clear();
        moves_.reserve(static_cast<size_t>(dp_table_[n].moves));

        frameStewart(n, 0, 3, 1, 2, 0);
        return static_cast<int>(moves_.size());
    }

    // -----------------------------------------------------------------------
    //  I/O Diagnostics
    // -----------------------------------------------------------------------
    void printDPTable() const {
        std::cout << "+-------+---------+-----------+\n"
                  << "| Disks | Split k |  FS moves |\n"
                  << "+-------+---------+-----------+\n";
        for (int i = 0; i <= max_disks_; ++i) {
            std::cout << "|  " << std::setw(3) << i
                      << "  |   " << std::setw(3) << dp_table_[i].k_split
                      << "   | " << std::setw(9) << dp_table_[i].moves << " |\n";
        }
        std::cout << "+-------+---------+-----------+\n";
    }

    void printMoves() const {
        for (size_t i = 0; i < moves_.size(); ++i) {
            const auto& m = moves_[i];
            std::cout << "  Move " << std::setw(3) << (i + 1)
                      << ":  Disk " << m.disk
                      << "  " << kPegLabel[m.from_peg]
                      << " -> " << kPegLabel[m.to_peg] << "\n";
        }
    }

private:
    /**
     * @struct DPState
     * @brief Cache-aligned DP table element merging moves and split indices.
     */
    struct DPState {
        int64_t moves;
        int k_split;
    };

    int max_disks_;
    std::vector<DPState> dp_table_;
    std::vector<Move> moves_;

    /**
     * @brief Precomputes the optimal k-split using standard Frame-Stewart recurrence.
     * Complexity: O(n^2) time, O(n) space.
     */
    void computeDP() {
        dp_table_[0] = {0, 0};
        if (max_disks_ >= 1) dp_table_[1] = {1, 0};

        for (int n = 2; n <= max_disks_; ++n) {
            int64_t best = std::numeric_limits<int64_t>::max();
            int best_k = 1;
            
            for (int k = 1; k <= n - 1; ++k) {
                int64_t cost = 2LL * dp_table_[k].moves + (1LL << (n - k)) - 1;
                if (cost < best) {
                    best = cost;
                    best_k = k;
                }
            }
            dp_table_[n] = {best, best_k};
        }
    }

    void recordMove(int disk, int from_peg, int to_peg) {
        moves_.push_back({disk, from_peg, to_peg});
    }

    /**
     * @brief Classic 3-peg Tower of Hanoi recursion.
     * Generates exact logical disk mappings using depth offset scaling.
     */
    void hanoi3(int n, int src, int tgt, int aux, int disk_offset) {
        if (n <= 0) return;
        hanoi3(n - 1, src, aux, tgt, disk_offset);
        recordMove(disk_offset + n, src, tgt);
        hanoi3(n - 1, aux, tgt, src, disk_offset);
    }

    /**
     * @brief Recursive 4-peg Frame-Stewart move generator.
     */
    void frameStewart(int n, int src, int tgt, int aux1, int aux2, int disk_offset) {
        if (n <= 0) return;
        if (n == 1) { 
            recordMove(disk_offset + 1, src, tgt); 
            return; 
        }
        
        int k = dp_table_[n].k_split;
        frameStewart(k, src, aux1, aux2, tgt, disk_offset);
        hanoi3(n - k, src, tgt, aux2, disk_offset + k);
        frameStewart(k, aux1, tgt, src, aux2, disk_offset);
    }
};

#endif // SOLVER_H
