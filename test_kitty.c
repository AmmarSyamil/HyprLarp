// compile: gcc -o test_kitty test_kitty.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// A simple 2x2 RGBA image (8 bytes of data)
// Pixels: Red, Green, Blue, White
uint8_t rgba_data[] = {
    0xFF, 0x00, 0x00, 0xFF, // Pixel 1: Red
    0x00, 0xFF, 0x00, 0xFF, // Pixel 2: Green
    0x00, 0x00, 0xFF, 0xFF, // Pixel 3: Blue
    0xFF, 0xFF, 0xFF, 0xFF  // Pixel 4: White
};

int main() {
    // Calculate base64 length (approx 4/3 of raw data, rounded up to multiple of 4)
    size_t data_len = 4 * 4;  // width(2)*height(2)*4 = 16 bytes
    size_t b64_len = ((data_len + 2) / 3) * 4;
    char *b64_data = malloc(b64_len + 1);
    
    if (!b64_data) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    // Use system base64 command (Ubuntu/Debian) for simplicity
    // For cross-platform, you'd use a proper base64 library
    FILE *proc = popen("base64", "w");
    if (!proc) {
        fprintf(stderr, "Failed to run base64 command\n");
        free(b64_data);
        return 1;
    }
    
    fwrite(rgba_data, 1, data_len, proc);
    fclose(proc);
    
    // In a real implementation, capture the base64 output properly.
    // For now, skip base64 and use a manual string for a red pixel.
    // A more reliable simple test: send a single red pixel
    printf("\x1b_Ga=T,f=32,s=1,v=1,t=d;"
           "/////w==" // base64 for a single white pixel for direct test
           "\x1b\\\n");
    fflush(stdout);
    
    return 0;
}