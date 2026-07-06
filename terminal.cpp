// Contain the function to read the response from the terminal

#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <poll.h>
#include <termios.h>
#include "DataType.hpp"
#include <termios.h>
#include <sys/ioctl.h>

// Set terminal to raw mode to read escape sequences instantly
bool set_terminal_raw(struct termios& original) {
    if (tcgetattr(STDIN_FILENO, &original) == -1) return false;
    
    struct termios raw = original;
    // Disable canonical mode (line buffering) and echo
    raw.c_lflag &= ~(ICANON | ECHO);
    // Set minimum read to 1 character, no timeout
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != -1;
}

// Restore original terminal settings
void restore_terminal(const struct termios& original) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}

// Function to read the response from the terminal with a timeout
std::string read_terminal_response(int timeout = 100) {
    struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
    std::string result;
    char ch;

    while (true) {
        int ret = poll(&pfd, 1, timeout);
        if (ret < 0) {
            std::cerr << "Error during poll: " << strerror(errno) << std::endl;
            break;
        } else if (ret == 0) {
            // Timeout - just break silently, it's normal
            break;
        } else {
            // Data available
            ssize_t bytesRead = read(STDIN_FILENO, &ch, 1);
            if (bytesRead <= 0) break;
            result += ch;
            if (ch == 't') break; // End of Kitty response
        }
    }

    return result;
}

// Function to parse the terminal response and extract height and width
bool parse_terminal_response(const std::string& resp, int& h, int& w) {
    size_t start = resp.find('[');
    if (start == std::string::npos) return false;
    size_t first_semi = resp.find(';', start);
    if (first_semi == std::string::npos) return false;
    size_t second_semi = resp.find(';', first_semi + 1);
    if (second_semi == std::string::npos) return false;

    try {
        h = std::stoi(resp.substr(first_semi + 1, second_semi - first_semi - 1));
        w = std::stoi(resp.substr(second_semi + 1, resp.find('t') - second_semi - 1));
        return true;
    } catch (...) { return false; }
}

// Function to query the terminal for its internal geometry (pixel dimensions and grid size)
bool query_terminal_internal_geometry(int& w, int& h, int& cols, int& rows) {
    struct termios original;
    if (!set_terminal_raw(original)) {
        std::cerr << "Failed to set terminal raw mode" << std::endl;
        return false;
    }

    // Flush stale input
    tcflush(STDIN_FILENO, TCIFLUSH);
    
    // Send queries
    std::cout << "\x1b[19t" << std::flush;  // text area in pixels
    std::cout << "\x1b[18t" << std::flush;  // grid size (rows/cols)

    // Read responses (give more time, 300ms is safe)
    std::string resp1 = read_terminal_response(300);
    std::string resp2 = read_terminal_response(300);

    // Restore terminal mode immediately after reading
    restore_terminal(original);

    // Parse
    if (!parse_terminal_response(resp1, h, w)) return false;
    if (!parse_terminal_response(resp2, rows, cols)) return false;

    return true;
}

// Internded method
int get_terminal_internal_geometry(int& text_w, int& text_h, int& cols, int& rows) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1) {
        std::cerr << "cant ioctl" << std::endl;
        return -1;
    }

    if (ws.ws_xpixel == 0 || ws.ws_ypixel == 0) {
        std::cerr << "no data" << std::endl;
        // Fallback: use the escape sequence method as a last resort,
        // but realistically, if Kitty returns 0, something is very wrong.
        return -1;
    }

    text_w = ws.ws_xpixel;
    text_h = ws.ws_ypixel;
    cols = ws.ws_col;
    rows = ws.ws_row;


    return 1;
}

// Function to return windows's terminal internal geommetry geometry
InternalTerminalGeometry GetInternalTerminalGeometry(const WindowPos& pos) {
    int w, h, col, row;
    
    // if (!query_terminal_internal_geometry(w, h, col, row)) {
    //     std::cerr << "Failed to query terminal internal geometry" << std::endl;
    //     return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    // }

    if (!get_terminal_internal_geometry(w, h, col, row)) {
        std::cerr << "Failed to get terminal internal geometry via ioctl" << std::endl;
        return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }

    // FIXED: Use size[0] and size[1] (dimensions), not at[0]/at[1] (screen coords)
    int pad_x = (pos.size[0] - w) / 2;
    int pad_y = pos.size[1] - h;

    int grid_screen_x = pos.at[0] + pad_x;
    int grid_screen_y = pos.at[1] + pad_y;

    int cell_w = w / col;
    int cell_h = h / row;

    // std::cout << "Internal Geometry: w=" << w << ", h=" << h 
    //           << ", cols=" << col << ", rows=" << row << std::endl;
    // std::cout << "Grid Screen Position: (" << grid_screen_x << ", " << grid_screen_y << ")" << std::endl;

    return {w, h, col, row, pad_x, pad_y, grid_screen_x, grid_screen_y, cell_w, cell_h};
}