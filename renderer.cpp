// renderer.cpp
#include <iostream>
#include <unistd.h>
#include <string>
#include <cstdio>

#include "base64converter.hpp"
#include <filesystem>
#include "DataType.hpp"
#include <cstdio>

// Function to display to write the escape sequences
int escSequence(int width, int height, std::string& imageSHM, const LayoutRender layoutRender, ViewportState vp) {
    // Check wether its should be rendering or not
    if (!vp.isRender) {
        // std::cerr << "EscSequence : ViewPortState is render is set to false" << std::endl;
        return 0; 
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

    // fprintf(stderr, "Sending escape seq\n");

    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "\x1b[%d;%dH\x1b_Ga=T,f=32,t=s,i=1,q=2,s=%d,v=%d,x=%d,y=%d,w=%d,h=%d,c=%d,r=%d,X=%d,Y=%d;%s\x1b\\",
        lr.cursor_row, lr.cursor_col, 
        width, height,
        lr.x, lr.y, lr.w, lr.h, 
        lr.disp_cols, lr.disp_rows,
        lr.sub_offset_x, lr.sub_offset_y, 
        b64_shm.c_str()
    );

    // Single write syscall
    write(STDOUT_FILENO, buf, n);
    fflush(stdout);
    return 1;
}



