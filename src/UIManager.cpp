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

#include <algorithm>
#include <csignal>
#include <cctype>
#include <fstream>
#include "UIManager.h"
#include "misc.h"

volatile sig_atomic_t UIManager::resized = 0;

UIManager::UIManager()
    : output_win(nullptr), header_win(nullptr), input_win(nullptr),
      term_height(0), term_width(0), scroll_offset(0), print(nullptr, this) {}

UIManager::~UIManager() {
    shutdown();
}

void UIManager::init() {
    initscr();
    if (has_colors()) {
        start_color();
        init_pair(NC_RED, COLOR_RED, COLOR_BLACK);
        init_pair(NC_BLUE, COLOR_BLUE, COLOR_BLACK);
        init_pair(NC_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    getmaxyx(stdscr, term_height, term_width);

    output_win = newwin(term_height - 4, term_width, 0, 0);
    header_win = newwin(1, term_width, term_height - 4, 0);
    input_win  = newwin(3, term_width, term_height - 3, 0);

    print = NcursesStream(output_win, this);

    scrollok(output_win, TRUE);
    box(input_win, 0, 0);
    keypad(input_win, TRUE);
    wtimeout(input_win, 100);

    wrefresh(output_win);
    wrefresh(input_win);
}

void UIManager::shutdown() {
    if (output_win) delwin(output_win);
    if (header_win) delwin(header_win);
    if (input_win) delwin(input_win);
    endwin();
}

void UIManager::resize() {
    endwin();
    refresh();
    clear();

    getmaxyx(stdscr, term_height, term_width);

    if (output_win) delwin(output_win);
    if (header_win) delwin(header_win);
    if (input_win) delwin(input_win);

    output_win = newwin(term_height - 4, term_width, 0, 0);
    header_win = newwin(1, term_width, term_height - 4, 0);
    input_win  = newwin(3, term_width, term_height - 3, 0);

    scrollok(output_win, TRUE);
    box(input_win, 0, 0);
    keypad(input_win, TRUE);
    wtimeout(input_win, 100);

    wrefresh(output_win);
    wrefresh(input_win);
    setHeader(currentHeader);
    UIManager::resized = 0;
}

void UIManager::redrawAll() {
    redrawOutput();
    redrawInput("", 3);
    setHeader(currentHeader);
}

void UIManager::redrawInput(const std::string& input_line, int cursor_x) {
    werase(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 1, "> %s", input_line.c_str());
    wmove(input_win, 1, cursor_x + input_line.length());
    wrefresh(input_win);
}

std::vector<std::string> UIManager::wrap_text(const std::string& text, int max_width) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start < text.size()) {
        size_t len = std::min((size_t)max_width, text.size() - start);
        lines.push_back(text.substr(start, len));
        start += len;
    }
    return lines;
}

void UIManager::redrawOutput() {
    werase(output_win);
    int win_height, win_width;
    getmaxyx(output_win, win_height, win_width);

    std::vector<std::pair<std::string, int>> wrapped_lines;
    for (const auto& line : log_lines) {
        auto parts = wrap_text(line.text, win_width);
        for (const auto& part : parts) {
            wrapped_lines.emplace_back(part, line.color_pair);
        }
    }

    int total_lines = wrapped_lines.size();
    int lines_to_show = std::min(win_height, total_lines);
    int start_line = std::max(0, total_lines - lines_to_show - scroll_offset);

    for (int i = 0; i < lines_to_show; ++i) {
        int idx = start_line + i;
        if (idx < 0 || idx >= (int)wrapped_lines.size()) continue;
        const auto& [text, color] = wrapped_lines[idx];
        if (color != 0)
            wattron(output_win, COLOR_PAIR(color));
        mvwaddnstr(output_win, i, 0, text.c_str(), win_width - 1);
        if (color != 0)
            wattroff(output_win, COLOR_PAIR(color));
    }
    wrefresh(output_win);
}

void UIManager::setHeader(const std::string& header) {
    werase(header_win);
    mvwprintw(header_win, 0, 1, "%s", header.c_str());
    wrefresh(header_win);
    currentHeader = header;
}

void UIManager::addLogLine(const std::string& line, int color) {
    size_t start = 0, end;
    while ((end = line.find('\n', start)) != std::string::npos) {
        std::string clean = line.substr(start, end - start);
        clean.erase(std::remove(clean.begin(), clean.end(), '\r'), clean.end());
        log_lines.push_back({clean, color});
        start = end + 1;
    }
    if (start < line.size()) {
        std::string clean = line.substr(start);
        clean.erase(std::remove(clean.begin(), clean.end(), '\r'), clean.end());
        log_lines.push_back({clean, color});
    }
    while (log_lines.size() > MAX_LOG_LINES) log_lines.erase(log_lines.begin());
}

void UIManager::clampScroll() {
    int win_height, win_width;
    getmaxyx(output_win, win_height, win_width);

    int wrapped_count = 0;
    for (const auto& line : log_lines) {
        auto parts = wrap_text(line.text, win_width);
        wrapped_count += parts.size();
    }

    int max_scroll = std::max(0, wrapped_count - win_height);
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    if (scroll_offset < 0) scroll_offset = 0;
}

// Returns key pressed, updates input_line, cursor_x, scroll_offset, and handles resize
int UIManager::getInput(std::string& input_line, int& cursor_x, bool& resized_flag) {
    if (resized) {
        resize();
        redrawOutput();
        redrawInput(input_line, cursor_x);
        resized_flag = true;
        return -1;
    }
    int ch = wgetch(input_win);
    resized_flag = false;
    return ch;
}

void UIManager::scrollUp(int lines) {
    scroll_offset += lines;
    clampScroll();
}
void UIManager::scrollDown(int lines) {
    scroll_offset -= lines;
    if (scroll_offset < 0) scroll_offset = 0;
}
void UIManager::scrollPageUp() {
    int win_height, win_width;
    getmaxyx(output_win, win_height, win_width);
    scrollUp(win_height / 2);
}
void UIManager::scrollPageDown() {
    int win_height, win_width;
    getmaxyx(output_win, win_height, win_width);
    scrollDown(win_height / 2);
}
void UIManager::scrollToBottom() {
    scroll_offset = 0;
}