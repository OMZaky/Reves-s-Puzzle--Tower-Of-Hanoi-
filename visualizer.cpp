// ============================================================================
//  visualizer.cpp — Animated Terminal Visualizer for Reve's Puzzle
// ============================================================================
//
//  Hooks into the Solver from solver.h, replays the precomputed move
//  sequence with an interactive ASCII playback loop in the terminal.
//
//  Build:  g++ -std=c++17 -O2 -o visualizer visualizer.cpp
//  Run:    ./visualizer [num_disks] [delay_ms]
//          Defaults: 8 disks, 400ms delay
// ============================================================================

#include "solver.h"

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>  // Required for `_kbhit()` and `_getch()` non-blocking input
#endif

// ---------------------------------------------------------------------------
//  ANSI escape code constants
// ---------------------------------------------------------------------------
namespace ansi {
    const char* RESET      = "\033[0m";
    const char* BOLD       = "\033[1m";
    const char* DIM        = "\033[2m";
    const char* CURSOR_HOME = "\033[H";     // move cursor to (1,1)
    const char* CLEAR      = "\033[2J";     // clear entire screen
    const char* HIDE_CUR   = "\033[?25l";   // hide cursor
    const char* SHOW_CUR   = "\033[?25h";   // show cursor

    // Foreground colors (bright variants for vivid disks)
    const char* FG_BLACK   = "\033[90m";
    const char* FG_WHITE   = "\033[97m";

    // Disk color palette — 12 vibrant 16-color standard codes (broad compatibility)
    const char* DISK_COLORS[] = {
        "\033[91m",   // bright red
        "\033[92m",   // bright green
        "\033[93m",   // bright yellow
        "\033[96m",   // bright cyan
        "\033[95m",   // bright magenta
        "\033[94m",   // bright blue
        "\033[31m",   // red
        "\033[32m",   // green
        "\033[33m",   // yellow
        "\033[36m",   // cyan
        "\033[35m",   // magenta
        "\033[34m",   // blue
    };
    constexpr int NUM_DISK_COLORS = 12;

    // Background highlight for the moved disk (Standard bright-black/dark-gray)
    const char* BG_HIGHLIGHT = "\033[100m";
}

// ---------------------------------------------------------------------------
//  Visualizer — renders the 4-peg state as an ASCII animation with user control
// ---------------------------------------------------------------------------
class Visualizer {
public:
    Visualizer(int num_disks, int delay_ms)
        : n_(num_disks),
          delay_ms_(delay_ms),
          // Calculate strict minimum bounding box width to prevent terminal line wraps 
          // (n_ '=' chars on each side + max digit string length + minimum 2 outer pad)
          col_width_(2 * num_disks + static_cast<int>(std::to_string(num_disks).length()) + 2), 
          pegs_(4)
    {
        // Pre-compute constant rendering strings (called hundreds of times per frame)
        int pad_left  = col_width_ / 2;
        int pad_right = col_width_ - pad_left - 1;
        std::ostringstream oss;
        oss << std::string(pad_left, ' ')
            << ansi::DIM << "|" << ansi::RESET
            << std::string(pad_right, ' ');
        pole_str_ = oss.str();
        base_str_ = std::string(col_width_, '=');
    }

    // -----------------------------------------------------------------------
    //  State Mutators
    // -----------------------------------------------------------------------
    void applyForward(const Move& m) {
        pegs_[m.from_peg].pop_back();
        pegs_[m.to_peg].push_back(m.disk);
    }

    void applyBackward(const Move& m) {
        pegs_[m.to_peg].pop_back();
        pegs_[m.from_peg].push_back(m.disk);
    }

    // -----------------------------------------------------------------------
    //  run() — setup solver, establish interaction loop, and route playback
    // -----------------------------------------------------------------------
    void run() {
        enableAnsiSupport();

        int total_width = col_width_ * 4 + 3 * 3;  // 4 cols + 3 gaps

        // Verify the terminal natively has enough horizontal margin to render
        if (isTerminalTooSmall(total_width)) {
            std::cout << "\n\033[91m [ERROR] Terminal window is too narrow for " << n_ << " disks!\n"
                      << "          Layout requires at least " << total_width << " columns horizontally.\n"
                      << "          Please maximize or resize your window now (waiting...)\033[0m" << std::flush;
            while (isTerminalTooSmall(total_width)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::cout << "\n\n\033[92m Window size OK! Starting simulation...\033[0m\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }

        // Generate optimal move logic structure decoupled from UI state
        Solver solver(n_);
        int total = solver.solve(n_);
        const auto& moves = solver.moves();

        // Initialize local peg state for rendering (disks n..1 on peg 0)
        for (auto& p : pegs_) p.clear();
        for (int d = n_; d >= 1; --d)
            pegs_[0].push_back(d);

        // Clear screen and hide cursor for clean animation
        std::cout << ansi::CLEAR << ansi::HIDE_CUR;
        
        // Render initial frame (m=0)
        render(-1, -1, 0, 0, total);

        bool paused = true;      // Start paused as requested by user
        int current_move = 0;    // Tracks our playback location
        bool keep_open = true;   // Tracks window lifetime

        auto last_update = std::chrono::steady_clock::now();

        // ===================================================================
        //  INTERACTIVE ASYNC GAME LOOP
        // ===================================================================
        while (keep_open) {
            
            // 1. Process User Input Non-Blockingly
#ifdef _WIN32
            if (_kbhit()) {
                int ch = _getch();
                
                // Windows arrow keys send a 224 prefix byte followed by a directional specifier byte
                if (ch == 224) {  
                    ch = _getch();
                    if (ch == 77 && current_move < total) { // Right Arrow -> Step Forward
                        applyForward(moves[current_move]);
                        current_move++;
                        render(moves[current_move-1].from_peg, moves[current_move-1].to_peg, moves[current_move-1].disk, current_move, total);
                        paused = true; // Auto-pause after an explicit manual step
                    } else if (ch == 75 && current_move > 0) { // Left Arrow -> Rewind
                        current_move--;
                        applyBackward(moves[current_move]);
                        // Highlight the preceding move geometry if rewinding mid-sequence
                        if (current_move > 0) {
                            render(moves[current_move-1].from_peg, moves[current_move-1].to_peg, moves[current_move-1].disk, current_move, total);
                        } else {
                            render(-1, -1, 0, 0, total); // Base state
                        }
                        paused = true;
                    }
                } 
                else if (ch == '\r') { // Enter/Return Key -> Toggle Play/Pause
                    paused = !paused;
                    // Reset the delta clock instantly when unpausing so it doesn't instantly snap a frame
                    last_update = std::chrono::steady_clock::now(); 
                } 
                else if (ch == 27 || ch == 'q' || ch == 'Q') { // Escape/Q Key -> Exit Safely
                    keep_open = false;
                }
            }
#endif

            // 2. Process Cinematic Animation Timing
            if (!paused && current_move < total) {
                auto now = std::chrono::steady_clock::now();
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update).count();

                // Has enough time elapsed to safely advance to the next frame?
                if (delta >= delay_ms_) {
                    applyForward(moves[current_move]);
                    current_move++;
                    render(moves[current_move-1].from_peg, moves[current_move-1].to_peg, moves[current_move-1].disk, current_move, total);
                    last_update = now;
                }
            }

            // 3. Halt explicitly on the final frame (Do not kill window)
            if (current_move == total && !paused) {
                paused = true; 
            }

            // Sleep exactly 10ms to prevent the infinite while(true) loop from burning a CPU core to 100% capacity
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Cleanup after dropping out of the interaction loop
        std::cout << ansi::SHOW_CUR;
        printSummary(total);
    }

private:
    int n_;           // number of disks
    int delay_ms_;    // animation delay per frame
    int col_width_;   // character width of each peg column
    std::vector<std::vector<int>> pegs_;  // local peg state for rendering
    std::string pole_str_;   // cached empty-pole render string
    std::string base_str_;   // cached base-line render string

    // -----------------------------------------------------------------------
    //  enableAnsiSupport — enable VT100 sequences on Windows 10+
    // -----------------------------------------------------------------------
    void enableAnsiSupport() {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return;
        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode)) return;
        SetConsoleMode(hOut, mode | 0x0004); // ENABLE_VIRTUAL_TERMINAL_PROCESSING
#endif
    }
    
    // -----------------------------------------------------------------------
    //  Checks if current screen exceeds terminal sizing to warn user of wrap
    // -----------------------------------------------------------------------
    bool isTerminalTooSmall(int essential_width) {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            return (csbi.srWindow.Right - csbi.srWindow.Left + 1) < essential_width;
        }
#endif
        return false; // Safely ignore in environments lacking terminal metrics
    }

    // -----------------------------------------------------------------------
    //  diskString — build the ASCII representation of a dynamic disk
    // -----------------------------------------------------------------------
    std::string diskString(int disk, bool highlight) const {
        std::string label = std::to_string(disk);
        int label_len = static_cast<int>(label.length());
        
        // Exact geometric symmetry algorithm (disk count establishes absolute '=' magnitude)
        int visual_width = 2 * disk + label_len;
        
        int pad_total = col_width_ - visual_width;
        int pad_left  = pad_total / 2;
        int pad_right = pad_total - pad_left;

        std::ostringstream oss;

        // Left padding
        oss << std::string(pad_left, ' ');

        // Disk color
        const char* color = ansi::DISK_COLORS[(disk - 1) % ansi::NUM_DISK_COLORS];
        if (highlight) oss << ansi::BG_HIGHLIGHT;
        oss << color << ansi::BOLD;

        // Left geometry
        oss << std::string(disk, '=');

        // Central Anchor point
        oss << ansi::FG_WHITE << label;

        // Right geometry
        oss << color << std::string(disk, '=');

        oss << ansi::RESET;

        // Right padding
        oss << std::string(pad_right, ' ');

        return oss.str();
    }

    // -----------------------------------------------------------------------
    //  poleString / baseString — return cached constant render strings
    // -----------------------------------------------------------------------
    const std::string& poleString() const { return pole_str_; }
    const std::string& baseString() const { return base_str_; }

    // -----------------------------------------------------------------------
    //  render — clear and render the current active playframe
    // -----------------------------------------------------------------------
    void render(int from_peg, int to_peg, int disk, int move_num, int total_moves) {
        std::ostringstream frame;

        // Cursor home — overwrite previous frame in place. 
        // Also add \033[0J (Clear from cursor to end of screen) to prevent cascading ghosting 
        // if the terminal window shrinks down.
        frame << ansi::CURSOR_HOME << "\033[0J";

        // ── Header ──────────────────────────────────────────────────────────
        int total_width = col_width_ * 4 + 3 * 3;  // 4 cols + 3 gaps of 3
        std::string border(total_width, '=');

        frame << "\n";
        frame << ansi::BOLD << ansi::FG_WHITE;
        frame << "  " << border << "\n";
        frame << "  ";

        // Title centered
        std::string title = "REVE'S PUZZLE  -  4-Peg Tower of Hanoi";
        int title_pad = (total_width - static_cast<int>(title.size())) / 2;
        frame << std::string(std::max(0, title_pad), ' ') << title << "\n";

        frame << "  " << border << "\n";
        frame << ansi::RESET;

        // ── Navigation Help & Move info bar ────────────────────────────────ـ
        frame << "\n";
        if (move_num == 0) {
            frame << "    " << ansi::BOLD << "\033[93m" << "[ENTER] Play/Pause    [->] Step Forward    [<-] Step Back    [ESC] Quit\n"
                  << ansi::RESET;
        } else {
            frame << "    " << ansi::BOLD << "Move " << move_num << " / " << total_moves
                  << ansi::RESET << "  |  ";

            // Show which disk moved where
            const char* dc = ansi::DISK_COLORS[(disk - 1) % ansi::NUM_DISK_COLORS];
            frame << dc << ansi::BOLD << "Disk " << disk << ansi::RESET
                  << "  " << ansi::BOLD
                  << Solver::kPegLabel[from_peg] << " -> " << Solver::kPegLabel[to_peg]
                  << ansi::RESET << "       \033[93m[Controls Available]\033[0m";

            // Small dynamic progress bar
            int bar_width = 30;
            int filled = (move_num * bar_width) / total_moves;
            frame << "   [";
            for (int b = 0; b < bar_width; ++b) {
                if (b < filled) frame << "\033[92m#" << ansi::RESET;  // Safe bright green
                else            frame << ansi::DIM << "-" << ansi::RESET; // Safe ASCII hyphen
            }
            frame << "]\n";
        }
        frame << "\n";

        // ── Pegs and Disks Layer Iteration ──────────────────────────────────
        std::string gap = "   ";
        int height = n_ + 1;  // Extra height room for visualization 

        for (int row = height; row >= 0; --row) {
            frame << "  ";
            for (int peg = 0; peg < 4; ++peg) {
                if (peg > 0) frame << gap;

                if (row < static_cast<int>(pegs_[peg].size())) {
                    int d = pegs_[peg][row];
                    bool highlight = (peg == to_peg && d == disk && move_num > 0);
                    frame << diskString(d, highlight);
                } else {
                    frame << poleString();
                }
            }
            frame << "\n";
        }

        // ── Bases ───────────────────────────────────────────────────────────
        frame << "  " << ansi::DIM;
        for (int peg = 0; peg < 4; ++peg) {
            if (peg > 0) frame << gap;
            frame << baseString();
        }
        frame << ansi::RESET << "\n";

        // ── Peg labels ──────────────────────────────────────────────────────
        frame << "  ";
        for (int peg = 0; peg < 4; ++peg) {
            if (peg > 0) frame << gap;
            int label_pad = col_width_ / 2 - 3;
            frame << std::string(std::max(0, label_pad), ' ')
                  << ansi::BOLD;

            // Highlight source/target pegs
            if (peg == to_peg && move_num > 0)
                frame << "\033[92m";   // green for target
            else if (peg == from_peg && move_num > 0)
                frame << "\033[91m";  // red for source
            else
                frame << ansi::FG_WHITE;

            frame << "Peg " << Solver::kPegLabel[peg]
                  << ansi::RESET
                  << std::string(std::max(0, col_width_ - label_pad - 5), ' ');
        }
        frame << "\n\n";

        // Blit full frame to terminal 
        std::cout << frame.str() << std::flush;
    }

    void printSummary(int total) {
        std::cout << "\n"
                  << ansi::BOLD << "\033[92m"
                  << "  * PLAYBACK TERMINATED" << ansi::RESET
                  << "  - " << n_ << " disks solved in "
                  << ansi::BOLD << total << " optimal moves" << ansi::RESET
                  << " (vs " << ((1 << n_) - 1) << " for 3-peg)\n\n";
    }
};

// ===========================================================================
//  main — parse optional CLI args and launch the visualizer
// ===========================================================================
int main(int argc, char* argv[]) {
    int num_disks = 0;
    int delay_ms  = 400;   // default delay

    if (argc >= 2) {
        num_disks = std::atoi(argv[1]);
    } else {
        std::cout << "Enter the number of disks (e.g., 8): ";
        if (!(std::cin >> num_disks) || num_disks < 1) {
            std::cerr << "Invalid input. Please enter a positive integer.\n";
            return 1;
        }
    }

    if (argc >= 3) {
        delay_ms  = std::atoi(argv[2]);
    }

    if (num_disks < 1 || num_disks > 20) {
        std::cerr << "Error: num_disks must be between 1 and 20.\n";
        return 1;
    }
    if (delay_ms < 10) delay_ms = 10; // Capped speed

    Visualizer vis(num_disks, delay_ms);
    vis.run();

    return 0;
}
