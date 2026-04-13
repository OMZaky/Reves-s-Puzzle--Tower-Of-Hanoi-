// ============================================================================
//  visualizer.cpp — Animated Terminal Visualizer for Reve's Puzzle
// ============================================================================
//
//  Consumes solver.h to replay the precomputed optimal move sequence as an
//  interactive, color-coded ASCII animation with full timeline controls.
//
//  Build:  g++ -std=c++17 -O2 -o visualizer visualizer.cpp
//  Run:    ./visualizer [num_disks] [delay_ms]
// ============================================================================

#include "solver.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#endif

// ---------------------------------------------------------------------------
//  ANSI escape code constants (constexpr for compile-time evaluation)
// ---------------------------------------------------------------------------
namespace ansi {
    constexpr const char* RESET       = "\033[0m";
    constexpr const char* BOLD        = "\033[1m";
    constexpr const char* DIM         = "\033[2m";
    constexpr const char* CURSOR_HOME = "\033[H";
    constexpr const char* CLEAR       = "\033[2J";
    constexpr const char* CLEAR_BELOW = "\033[0J";
    constexpr const char* HIDE_CUR    = "\033[?25l";
    constexpr const char* SHOW_CUR    = "\033[?25h";

    constexpr const char* FG_WHITE    = "\033[97m";
    constexpr const char* FG_RED      = "\033[91m";
    constexpr const char* FG_GREEN    = "\033[92m";
    constexpr const char* FG_YELLOW   = "\033[93m";
    constexpr const char* BG_HIGHLIGHT = "\033[100m";

    /// 12-color palette for disk rendering (cycled for disk > 12).
    constexpr const char* DISK_COLORS[] = {
        "\033[91m", "\033[92m", "\033[93m", "\033[96m",
        "\033[95m", "\033[94m", "\033[31m", "\033[32m",
        "\033[33m", "\033[36m", "\033[35m", "\033[34m",
    };
    constexpr int NUM_DISK_COLORS = 12;
}

// ---------------------------------------------------------------------------
//  Visualizer
// ---------------------------------------------------------------------------
class Visualizer {
public:
    Visualizer(int num_disks, int delay_ms)
        : n_(num_disks),
          delay_ms_(delay_ms),
          col_width_(2 * num_disks
                     + static_cast<int>(std::to_string(num_disks).length()) + 2),
          pegs_(Solver::kNumPegs)
    {
        precacheDiskStrings();
        precacheLayoutStrings();
    }

    /// Sets up the solver, initializes peg state, and enters the event loop.
    void run() {
        enableAnsiSupport();
        waitForTerminalSize();

        // Generate optimal move sequence (algorithm layer)
        Solver solver(n_);
        const int total = solver.solve(n_);
        const auto& moves = solver.moves();

        // Initialize rendering state: disks n..1 on peg 0
        resetPegs();

        std::cout << ansi::CLEAR << ansi::HIDE_CUR;
        render(-1, -1, 0, 0, total);

        runEventLoop(moves, total);

        std::cout << ansi::SHOW_CUR;
        printSummary(total);
    }

private:
    int n_;
    int delay_ms_;
    int col_width_;
    std::vector<std::vector<int>> pegs_;

    // Pre-cached render strings (computed once, referenced every frame)
    std::string              pole_str_;
    std::string              base_str_;
    std::vector<std::string> disk_strs_;              // disk_strs_[d-1]
    std::vector<std::string> disk_strs_highlighted_;   // highlighted variant

    // -----------------------------------------------------------------------
    //  Pre-caching — eliminates all per-frame string construction
    // -----------------------------------------------------------------------

    /// Builds the constant pole and base-line strings.
    void precacheLayoutStrings() {
        int pad_left  = col_width_ / 2;
        int pad_right = col_width_ - pad_left - 1;

        std::ostringstream oss;
        oss << std::string(pad_left, ' ')
            << ansi::DIM << "|" << ansi::RESET
            << std::string(pad_right, ' ');
        pole_str_ = oss.str();
        base_str_ = std::string(col_width_, '=');
    }

    /// Pre-renders all disk strings (normal + highlighted) for disks [1, n_].
    void precacheDiskStrings() {
        disk_strs_.resize(n_);
        disk_strs_highlighted_.resize(n_);
        for (int d = 1; d <= n_; ++d) {
            disk_strs_[d - 1]             = buildDiskString(d, false);
            disk_strs_highlighted_[d - 1] = buildDiskString(d, true);
        }
    }

    /// Constructs the ANSI-colored ASCII representation of a single disk.
    std::string buildDiskString(int disk, bool highlight) const {
        const std::string label = std::to_string(disk);
        const int label_len     = static_cast<int>(label.length());
        const int vis_width     = 2 * disk + label_len;
        const int pad_total     = col_width_ - vis_width;
        const int pad_left      = pad_total / 2;
        const int pad_right     = pad_total - pad_left;
        const char* color       = ansi::DISK_COLORS[(disk - 1) % ansi::NUM_DISK_COLORS];

        std::ostringstream oss;
        oss << std::string(pad_left, ' ');
        if (highlight) oss << ansi::BG_HIGHLIGHT;
        oss << color << ansi::BOLD
            << std::string(disk, '=')
            << ansi::FG_WHITE << label
            << color << std::string(disk, '=')
            << ansi::RESET
            << std::string(pad_right, ' ');
        return oss.str();
    }

    // -----------------------------------------------------------------------
    //  State management
    // -----------------------------------------------------------------------

    void resetPegs() {
        for (auto& p : pegs_) p.clear();
        for (int d = n_; d >= 1; --d)
            pegs_[0].push_back(d);
    }

    void applyForward(const Move& m) {
        pegs_[m.from_peg].pop_back();
        pegs_[m.to_peg].push_back(m.disk);
    }

    void applyBackward(const Move& m) {
        pegs_[m.to_peg].pop_back();
        pegs_[m.from_peg].push_back(m.disk);
    }

    // -----------------------------------------------------------------------
    //  Platform utilities
    // -----------------------------------------------------------------------

    static void enableAnsiSupport() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return;
        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode)) return;
        SetConsoleMode(hOut, mode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
    }

    bool isTerminalWideEnough(int required_width) const {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi))
            return (csbi.srWindow.Right - csbi.srWindow.Left + 1) >= required_width;
#else
        (void)required_width;
#endif
        return true;
    }

    void waitForTerminalSize() const {
        const int total_width = col_width_ * Solver::kNumPegs + 3 * (Solver::kNumPegs - 1);
        if (isTerminalWideEnough(total_width)) return;

        std::cout << "\n" << ansi::FG_RED << ansi::BOLD
                  << " [ERROR] Terminal too narrow for " << n_ << " disks!\n"
                  << "         Need at least " << total_width << " columns.\n"
                  << "         Please resize now (waiting...)"
                  << ansi::RESET << std::flush;

        while (!isTerminalWideEnough(total_width))
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << "\n\n" << ansi::FG_GREEN
                  << " Window size OK! Starting simulation..."
                  << ansi::RESET << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }

    // -----------------------------------------------------------------------
    //  Event loop — input polling, animation timing, frame dispatch
    // -----------------------------------------------------------------------

    /// Applies a move step (forward or backward) and renders the result.
    void stepForward(const std::vector<Move>& moves, int& current_move, int total) {
        applyForward(moves[current_move]);
        ++current_move;
        const auto& m = moves[current_move - 1];
        render(m.from_peg, m.to_peg, m.disk, current_move, total);
    }

    void stepBackward(const std::vector<Move>& moves, int& current_move, int total) {
        --current_move;
        applyBackward(moves[current_move]);
        if (current_move > 0) {
            const auto& m = moves[current_move - 1];
            render(m.from_peg, m.to_peg, m.disk, current_move, total);
        } else {
            render(-1, -1, 0, 0, total);
        }
    }

    /// Polls non-blocking keyboard input. Returns false if user requests exit.
    bool processInput(const std::vector<Move>& moves, int total,
                      int& current_move, bool& paused,
                      std::chrono::steady_clock::time_point& last_update)
    {
#ifdef _WIN32
        if (!_kbhit()) return true;

        int ch = _getch();

        // Windows arrow keys emit a 224 prefix byte + direction byte
        if (ch == 224) {
            ch = _getch();
            if (ch == 77 && current_move < total) {           // Right → Step Forward
                stepForward(moves, current_move, total);
                paused = true;
            } else if (ch == 75 && current_move > 0) {        // Left  → Step Backward
                stepBackward(moves, current_move, total);
                paused = true;
            }
        }
        else if (ch == '\r') {                                 // Enter → Toggle Play/Pause
            paused = !paused;
            last_update = std::chrono::steady_clock::now();
        }
        else if (ch == 27 || ch == 'q' || ch == 'Q') {        // Esc/Q → Exit
            return false;
        }
#else
        (void)moves; (void)total; (void)current_move;
        (void)paused; (void)last_update;
#endif
        return true;
    }

    /// Core event loop: polls input, advances animation on timer ticks.
    void runEventLoop(const std::vector<Move>& moves, int total) {
        bool paused       = true;
        int  current_move = 0;
        auto last_update  = std::chrono::steady_clock::now();

        while (true) {
            // 1. Handle user input
            if (!processInput(moves, total, current_move, paused, last_update))
                break;

            // 2. Advance animation when playing
            if (!paused && current_move < total) {
                auto now   = std::chrono::steady_clock::now();
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now - last_update).count();

                if (delta >= delay_ms_) {
                    stepForward(moves, current_move, total);
                    last_update = now;
                }
            }

            // 3. Auto-pause on final frame
            if (current_move == total && !paused)
                paused = true;

            // 4. Yield CPU (prevents busy-spin at 100% core usage)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // -----------------------------------------------------------------------
    //  Rendering — stateless frame composition (reads pegs_, writes stdout)
    // -----------------------------------------------------------------------

    void render(int from_peg, int to_peg, int disk,
                int move_num, int total_moves) const
    {
        const int total_width = col_width_ * Solver::kNumPegs
                              + 3 * (Solver::kNumPegs - 1);
        const std::string border(total_width, '=');
        const std::string gap = "   ";

        std::ostringstream frame;
        frame << ansi::CURSOR_HOME << ansi::CLEAR_BELOW;

        renderHeader(frame, border, total_width);
        renderInfoBar(frame, from_peg, to_peg, disk, move_num, total_moves);
        renderTowers(frame, gap, to_peg, disk, move_num);
        renderBases(frame, gap);
        renderPegLabels(frame, gap, from_peg, to_peg, move_num);

        std::cout << frame.str() << std::flush;
    }

    void renderHeader(std::ostringstream& frame,
                      const std::string& border, int total_width) const
    {
        const std::string title = "REVE'S PUZZLE  -  4-Peg Tower of Hanoi";
        const int title_pad = (total_width - static_cast<int>(title.size())) / 2;

        frame << "\n"
              << ansi::BOLD << ansi::FG_WHITE
              << "  " << border << "\n"
              << "  " << std::string(std::max(0, title_pad), ' ') << title << "\n"
              << "  " << border << "\n"
              << ansi::RESET;
    }

    void renderInfoBar(std::ostringstream& frame, int from_peg, int to_peg,
                       int disk, int move_num, int total_moves) const
    {
        frame << "\n";
        if (move_num == 0) {
            frame << "    " << ansi::BOLD << ansi::FG_YELLOW
                  << "[ENTER] Play/Pause    [->] Step Forward    "
                  << "[<-] Step Back    [ESC] Quit\n"
                  << ansi::RESET;
        } else {
            const char* dc = ansi::DISK_COLORS[(disk - 1) % ansi::NUM_DISK_COLORS];
            frame << "    " << ansi::BOLD
                  << "Move " << move_num << " / " << total_moves
                  << ansi::RESET << "  |  "
                  << dc << ansi::BOLD << "Disk " << disk << ansi::RESET
                  << "  " << ansi::BOLD
                  << Solver::kPegLabel[from_peg] << " -> "
                  << Solver::kPegLabel[to_peg]
                  << ansi::RESET
                  << "       " << ansi::FG_YELLOW << "[Controls Available]"
                  << ansi::RESET;

            // Progress bar
            constexpr int kBarWidth = 30;
            const int filled = (move_num * kBarWidth) / total_moves;
            frame << "   [";
            for (int b = 0; b < kBarWidth; ++b) {
                if (b < filled) frame << ansi::FG_GREEN << "#" << ansi::RESET;
                else            frame << ansi::DIM << "-" << ansi::RESET;
            }
            frame << "]\n";
        }
        frame << "\n";
    }

    void renderTowers(std::ostringstream& frame, const std::string& gap,
                      int to_peg, int disk, int move_num) const
    {
        const int height = n_ + 1;
        for (int row = height; row >= 0; --row) {
            frame << "  ";
            for (int peg = 0; peg < Solver::kNumPegs; ++peg) {
                if (peg > 0) frame << gap;

                if (row < static_cast<int>(pegs_[peg].size())) {
                    int  d         = pegs_[peg][row];
                    bool highlight = (peg == to_peg && d == disk && move_num > 0);
                    frame << (highlight ? disk_strs_highlighted_[d - 1]
                                        : disk_strs_[d - 1]);
                } else {
                    frame << pole_str_;
                }
            }
            frame << "\n";
        }
    }

    void renderBases(std::ostringstream& frame, const std::string& gap) const {
        frame << "  " << ansi::DIM;
        for (int peg = 0; peg < Solver::kNumPegs; ++peg) {
            if (peg > 0) frame << gap;
            frame << base_str_;
        }
        frame << ansi::RESET << "\n";
    }

    void renderPegLabels(std::ostringstream& frame, const std::string& gap,
                         int from_peg, int to_peg, int move_num) const
    {
        frame << "  ";
        for (int peg = 0; peg < Solver::kNumPegs; ++peg) {
            if (peg > 0) frame << gap;
            const int label_pad = col_width_ / 2 - 3;
            frame << std::string(std::max(0, label_pad), ' ')
                  << ansi::BOLD;

            if      (peg == to_peg   && move_num > 0) frame << ansi::FG_GREEN;
            else if (peg == from_peg && move_num > 0) frame << ansi::FG_RED;
            else                                      frame << ansi::FG_WHITE;

            frame << "Peg " << Solver::kPegLabel[peg]
                  << ansi::RESET
                  << std::string(std::max(0, col_width_ - label_pad - 5), ' ');
        }
        frame << "\n\n";
    }

    void printSummary(int total) const {
        std::cout << "\n"
                  << ansi::BOLD << ansi::FG_GREEN
                  << "  * PLAYBACK TERMINATED" << ansi::RESET
                  << "  - " << n_ << " disks solved in "
                  << ansi::BOLD << total << " optimal moves" << ansi::RESET
                  << " (vs " << ((1 << n_) - 1) << " for 3-peg)\n\n";
    }
};

// ===========================================================================
//  Entry point
// ===========================================================================

int main(int argc, char* argv[]) {
    int num_disks = 0;
    int delay_ms  = 400;

    if (argc >= 2) {
        num_disks = std::atoi(argv[1]);
    } else {
        std::cout << "Enter the number of disks (e.g., 8): ";
        if (!(std::cin >> num_disks) || num_disks < 1) {
            std::cerr << "Invalid input. Please enter a positive integer.\n";
            return 1;
        }
    }

    if (argc >= 3)
        delay_ms = std::atoi(argv[2]);

    if (num_disks < 1 || num_disks > 20) {
        std::cerr << "Error: num_disks must be between 1 and 20.\n";
        return 1;
    }
    if (delay_ms < 10) delay_ms = 10;

    Visualizer vis(num_disks, delay_ms);
    vis.run();

    return 0;
}
