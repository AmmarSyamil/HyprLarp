// renderer.cpp
#include <iostream>
#include <unistd.h>
#include <string>
#include <cstdio>
#include "base64converter.hpp"
#include <filesystem>
#include "DataType.hpp"
#include <cstdio>
#include <fstream>
#include <cstring>
#include "terminal.hpp"

void hb(const char* where) {
    static std::ofstream dbg("/tmp/hyprlarp_heartbeat.log", std::ios::app);
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    dbg << ms << " " << where << std::endl;
    dbg.flush();
}

bool writeAll(int fd, const char* buf, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(fd, buf + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue; // only matters if fd is non-blocking
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

// Function to display to write the escape sequences
int escSequence(int width, int height, std::string& imageSHM, const LayoutRender layoutRender, ViewportState vp) {
    // Check wether its should be rendering or not
    if (!vp.isRender) {
        // std::cerr << "EscSequence : ViewPortState is render is set to false" << std::endl;
        return 0; 
    }
    if (layoutRender.w <= 0 || layoutRender.h <= 0 || layoutRender.disp_cols <= 0 || layoutRender.disp_rows <= 0) {
        return 0;   // skip this frame
    }

    std::string SHMfileName = imageSHM;
    std::string real_path = "/dev/shm/" + SHMfileName;
    if (std::filesystem::exists(real_path)) {
//             std::cout << "The file exists at for pre display " << real_path << " right now!" << std::endl;
    } else {
        std::cerr << "The file is genuinely not at pre display " << real_path << std::endl;
    }

    // Convert to base64 for kitty
    std::string b64_shm_name = base64Converter(SHMfileName);

    // Move the cursor to theexact row and column cell
    std::string move_cursor = "\x1b[" + std::to_string(layoutRender.cursor_row) + ";" 
                                      + std::to_string(layoutRender.cursor_col) + "H";
    // write(STDOUT_FILENO, move_cursor.c_str(), move_cursor.size());

    // Create the escape sequence string
    // std::string escape = "\x1b_Ga=T,f=32,s=" + std::to_string(width) + 
    //                  ",v=" + std::to_string(height) + 
    //                  ",t=s,O=0,S=" + std::to_string(pixel_data_sizes) +
    //                  ",i=1"
    //                  ",q=2;" + b64_shm_name + "\x1b\\";

    std::string escape = 
        "\x1b_G"
        "a=T"                     // Action: Transmit and display
        ",f=32"                   // Format: 32-bit RGBA
        ",t=s"                    // Transmission: Shared Memory
        ",d=a"                    // NOT DELETE SHM (WTF I SHOULD HAVE KNOWN THIS EARLIER)
        ",i=1"                    // Re-use Image ID 1 (prevents GPU memory leaks)
        ",q=2"                    // Quiet mode (mutes terminal confirmations)
        ",s=" + std::to_string(width) +   // Total width of the raw SHM buffer frame
        ",v=" + std::to_string(height) +  // Total height of the raw SHM buffer frame
        ",x=" + std::to_string(layoutRender.x) +    // Start X column of the source video crop
        ",y=" + std::to_string(layoutRender.y) +    // Start Y row of the source video crop
        ",w=" + std::to_string(layoutRender.w) +    // Width size of our source video crop
        ",h=" + std::to_string(layoutRender.h) +    // Height size of our source video crop
        ",c=" + std::to_string(layoutRender.disp_cols) + // Columns of text grid space to span on screen
        ",r=" + std::to_string(layoutRender.disp_rows) + // Rows of text grid space to span on screen
        ",X=" + std::to_string(layoutRender.sub_offset_x) + // Fractional X pixel nudge inside the cell
        ",Y=" + std::to_string(layoutRender.sub_offset_y) + // Fractional Y pixel nudge inside the cell
        ";" + b64_shm_name +      // The base64 name of our shared memory segment
        "\x1b\\";                 // Close the sequence

    // Cursor
    // write(STDOUT_FILENO, "\x1b[H", 3); // Move cursor to top-left corner

    std::string final_render_packet = move_cursor + escape;
    
    // Using writeFirst pixel byte:
    write(STDOUT_FILENO, final_render_packet.c_str(), final_render_packet.size());
    fflush(stdout);
    // fprintf(stderr, "\n");
    
    return 1;
}

// Version 2
int escSequence(int width, int height, const std::string& b64_shm, const LayoutRender& lr, const ViewportState& vp) {
    if (!vp.isRender) return 0;

    if (lr.w <= 0 || lr.h <= 0 || lr.disp_cols <= 0 || lr.disp_rows <= 0) {
        return 0;
    }

    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "\x1b[%d;%dH\x1b_Ga=T,f=32,t=s,d=a,i=1,q=2,s=%d,v=%d,x=%d,y=%d,w=%d,h=%d,c=%d,r=%d,X=%d,Y=%d;%s\x1b\\",
        lr.cursor_row, lr.cursor_col,
        width, height,
        lr.x, lr.y, lr.w, lr.h,
        lr.disp_cols, lr.disp_rows,
        lr.sub_offset_x, lr.sub_offset_y,
        b64_shm.c_str()
    );

    // DEBUG: log every call, once per second is enough to not flood the file
    static int dbgcount = 0;
    if (++dbgcount % 60 == 0) {
        std::ofstream dbg("/tmp/hyprlarp_esc_debug.log", std::ios::app);
        dbg << "n=" << n << " buflen_ok=" << (n >= 0 && (size_t)n < sizeof(buf))
            << " b64_shm_len=" << b64_shm.size()
            << " row=" << lr.cursor_row << " col=" << lr.cursor_col
            << std::endl;
    }

    if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
        std::cerr << "escSequence: snprintf truncated or failed, n=" << n << std::endl;
        return 0;
    }

    

    hb("before_write");
    if (!writeAll(STDOUT_FILENO, buf, static_cast<size_t>(n))) {
        std::cerr << "escSequence: write failed, errno=" << errno << " (" << strerror(errno) << ")" << std::endl;
        return 0;
    }
    hb("after_write");
    fflush(stdout);
    return 1;
}