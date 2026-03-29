/**
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of

 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#pragma once

#include <ncurses.h>
#include <string>
#include <vector>
#include <sstream>
#include <csignal>
#include <mutex>

struct ColoredLine {
    std::string text;
    int color_pair;
};

// Color enums for clean usage
enum NcColor {
    NC_DEFAULT = 0,
    NC_RED     = 1,
    NC_BLUE    = 2,
    NC_YELLOW  = 3
};

namespace detail {
// Per-thread buffers so ui.print from the receive thread cannot interleave with the main thread.
inline std::ostringstream& ncurses_tls_buf() {
    thread_local std::ostringstream buf;
    return buf;
}
inline int& ncurses_tls_color() {
    thread_local int c = NC_DEFAULT;
    return c;
}
}  // namespace detail

class UIManager {
private:
    WINDOW* output_win;
    WINDOW* header_win;
    WINDOW* input_win;
    int term_height, term_width;
    std::vector<ColoredLine> log_lines;
    int scroll_offset;
    std::string currentHeader;
    mutable std::recursive_mutex display_mutex;
    bool output_dirty = false;

    void pushLogLineUnlocked(const std::string& line, int color);
    std::vector<std::string> wrap_text(const std::string& text, int max_width);

    public:
    UIManager();
    ~UIManager();

    void init();
    void shutdown();

    void resize();
    void redrawAll();
    void redrawInput(const std::string& input_line, int cursor_x = 3);
    void redrawOutput(bool force = false);
    void setHeader(const std::string& header);

    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);
    void scrollPageUp();
    void scrollPageDown();
    void scrollToBottom();

    int getInput(std::string& input_line, int& cursor_x, bool& resized);

    void clampScroll();

    // Expose for signal handler
    static volatile sig_atomic_t resized;

    class NcursesStream {
        UIManager* parent;

    public:
        explicit NcursesStream(UIManager* p) : parent(p) { }

        // Allow print(RED) syntax
        NcursesStream& operator()(int color) {
            std::lock_guard<std::recursive_mutex> lock(parent->display_mutex);
            detail::ncurses_tls_color() = color;
            return *this;
        }

        // Handle any streamed type
        template <typename T>
        NcursesStream& operator<<(const T& val) {
            std::lock_guard<std::recursive_mutex> lock(parent->display_mutex);
            detail::ncurses_tls_buf() << val;
            return *this;
        }

        // Handle std::endl
        NcursesStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
            if (manip == static_cast<std::ostream& (*)(std::ostream&)>(std::endl)) {
                std::lock_guard<std::recursive_mutex> lock(parent->display_mutex);
                std::string line = detail::ncurses_tls_buf().str();
                detail::ncurses_tls_buf().str("");
                detail::ncurses_tls_buf().clear();
                int col = detail::ncurses_tls_color();
                detail::ncurses_tls_color() = NC_DEFAULT;
                parent->pushLogLineUnlocked(line, col);
                parent->scroll_offset = 0;
                parent->output_dirty = true;
            }
            return *this;
        }
    };

    NcursesStream print;

    friend class NcursesStream;
};