// Contain the function to read the response from the terminal
// terminal.cpp
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
#include "terminal.hpp"
#include <chrono>
#include <sstream>
#include <fstream>
#include <vector>
#include "socketSend.hpp"

static void hb(const char* where) {
    static std::ofstream dbg("/tmp/hyprlarp_heartbeat.log", std::ios::app);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    dbg << ms << " " << where << std::endl;
    dbg.flush();
}

struct KittyPadding {
    int left   = -1; // -1 means not set
    int top    = -1;
    int right  = -1;
    int bottom = -1;
};

static KittyPadding g_kitty_padding;
static bool g_kitty_padding_loaded = false;
static int g_border_size = 0;
static int g_gaps_in = 0;
static bool g_hyprland_options_loaded = false;

// Helper to trim whitespace
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Split a string by whitespace into tokens
static std::vector<std::string> splitTokens(const std::string& s) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (ss >> token) tokens.push_back(token);
    return tokens;
}

static void loadHyprlandOptions() {
    if (g_hyprland_options_loaded) return;

    nlohmann::json json;
    // Get border size (correct option name)
    if (GetHyprlandOption("general:border_size", json) == 0) {
        // The value is in the "int" field for numeric options
        if (json.contains("int") && json["int"].is_number_integer()) {
            g_border_size = json["int"].get<int>();
        } else if (json.contains("value")) {
            // Fallback for string values
            if (json["value"].is_number_integer())
                g_border_size = json["value"].get<int>();
            else if (json["value"].is_string())
                g_border_size = std::stoi(json["value"].get<std::string>());
        }
    }
    // Get gaps_in (correct option name)
    if (GetHyprlandOption("general:gaps_in", json) == 0) {
        if (json.contains("int") && json["int"].is_number_integer()) {
            g_gaps_in = json["int"].get<int>();
        } else if (json.contains("value")) {
            if (json["value"].is_number_integer())
                g_gaps_in = json["value"].get<int>();
            else if (json["value"].is_string()) {
                std::string val = json["value"].get<std::string>();
                // If it has spaces, take the first number
                size_t space = val.find(' ');
                if (space != std::string::npos)
                    val = val.substr(0, space);
                g_gaps_in = std::stoi(val);
            }
        }
    }

    g_hyprland_options_loaded = true;
}


// Parse a single line of Kitty config to extract padding values
static void parsePaddingLine(const std::string& line, KittyPadding& pad) {
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') return;

    auto tokens = splitTokens(trimmed);
    if (tokens.empty()) return;

    std::string key = tokens[0];
    if (key == "window_padding_width") {
        // 1, 2, or 4 integers
        if (tokens.size() < 2) return;
        int vals[4] = {0,0,0,0};
        size_t count = tokens.size() - 1;
        for (size_t i = 0; i < count && i < 4; ++i) {
            vals[i] = std::stoi(tokens[i+1]);
        }
        if (count == 1) {
            // all sides same
            pad.left = pad.top = pad.right = pad.bottom = vals[0];
        } else if (count == 2) {
            // left/right = vals[0], top/bottom = vals[1]
            pad.left = pad.right = vals[0];
            pad.top = pad.bottom = vals[1];
        } else if (count == 4) {
            // left, top, right, bottom
            pad.left = vals[0];
            pad.top = vals[1];
            pad.right = vals[2];
            pad.bottom = vals[3];
        }
    } else if (key == "window_padding_left") {
        if (tokens.size() >= 2) pad.left = std::stoi(tokens[1]);
    } else if (key == "window_padding_right") {
        if (tokens.size() >= 2) pad.right = std::stoi(tokens[1]);
    } else if (key == "window_padding_top") {
        if (tokens.size() >= 2) pad.top = std::stoi(tokens[1]);
    } else if (key == "window_padding_bottom") {
        if (tokens.size() >= 2) pad.bottom = std::stoi(tokens[1]);
    }
}

// Read Kitty config from file path and fill padding struct
static bool readKittyConfigFile(const std::string& path, KittyPadding& pad) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        parsePaddingLine(line, pad);
    }
    return true;
}

// Get Kitty padding from the config, following priority order
KittyPadding getKittyPadding() {
    KittyPadding pad;

    // 1. Check KITTY_CONFIG_DIRECTORY environment variable
    const char* kittyConfigDir = std::getenv("KITTY_CONFIG_DIRECTORY");
    if (kittyConfigDir) {
        std::string path = std::string(kittyConfigDir) + "/kitty.conf";
        if (readKittyConfigFile(path, pad)) return pad;
    }

    // 2. Check XDG_CONFIG_HOME
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome) {
        std::string path = std::string(xdgConfigHome) + "/kitty/kitty.conf";
        if (readKittyConfigFile(path, pad)) return pad;
    }

    // 3. Check ~/.config/kitty/kitty.conf
    const char* home = std::getenv("HOME");
    if (home) {
        std::string path = std::string(home) + "/.config/kitty/kitty.conf";
        if (readKittyConfigFile(path, pad)) return pad;
    }

    // 4. Check XDG_CONFIG_DIRS (colon-separated list)
    const char* xdgConfigDirs = std::getenv("XDG_CONFIG_DIRS");
    if (xdgConfigDirs) {
        std::string dirs = xdgConfigDirs;
        std::stringstream ss(dirs);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (dir.empty()) continue;
            std::string path = dir + "/kitty/kitty.conf";
            if (readKittyConfigFile(path, pad)) return pad;
        }
    }

    // 5. Fallback to /etc/xdg/kitty/kitty.conf
    if (readKittyConfigFile("/etc/xdg/kitty/kitty.conf", pad)) return pad;

    // If nothing found, return default values (will be ignored by caller)
    return pad;
}

// Function to load padding (call once)
static void loadKittyPadding() {
    if (g_kitty_padding_loaded) return;
    g_kitty_padding = getKittyPadding();
    g_kitty_padding_loaded = true;
}

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
std::string read_terminal_response(int timeout) {
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

    hb("before_ioctl");
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1) { hb("after_ioctl_failed");}
    hb("after_ioctl");

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
    if (get_terminal_internal_geometry(w, h, col, row) != 1) {
        return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    }

    int cell_w = w / col;
    int cell_h = h / row;

    // Load Kitty padding and Hyprland options
    loadKittyPadding();
    loadHyprlandOptions();   // <-- new

    int pad_x, pad_y;
    bool hasKittyPadding = (g_kitty_padding.left >= 0 && g_kitty_padding.top >= 0 &&
                            g_kitty_padding.right >= 0 && g_kitty_padding.bottom >= 0);

    if (hasKittyPadding && g_hyprland_options_loaded) {
        // Use real decorations
        int border = g_border_size;
        int gap    = g_gaps_in;

        // Horizontal: left decoration = border + gap + Kitty left padding
        pad_x = border + gap + g_kitty_padding.left;

        // Vertical: compute top offset from bottom decorations
        int bottom_deco = border + gap + g_kitty_padding.bottom;
        int total_extra = pos.size[1] - h;
        int top_deco = total_extra - bottom_deco;
        if (top_deco < 0) top_deco = 0;   // safety
        pad_y = top_deco;
    } else {
        // Fallback to inference (your previous logic)
        int total_padding = pos.size[1] - h;
        int bottom_remainder = total_padding % cell_h;
        int actual_top_padding = total_padding - bottom_remainder;
        pad_y = (actual_top_padding >= 0) ? actual_top_padding : total_padding;
        pad_x = (pos.size[0] - w) / 2;
    }

    int grid_screen_x = pos.at[0] + pad_x;
    int grid_screen_y = pos.at[1] + pad_y;

    return {w, h, col, row, pad_x, pad_y, grid_screen_x, grid_screen_y, cell_w, cell_h};
}