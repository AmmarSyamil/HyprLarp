#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <cstring>
#include <string>
#include <cstdint>

// Minimal base64 encoder (for a short string)
std::string base64_encode(const std::string& input) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

int main() {
    const char* shm_name = "/test_shm_mini";
    const int width = 2, height = 2;
    const int pixel_data_size = width * height * 4; // 16 bytes

    // Create SHM
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0644);
    if (fd == -1) { perror("shm_open"); return 1; }
    if (ftruncate(fd, pixel_data_size) == -1) { perror("ftruncate"); return 1; }

    void* ptr = mmap(NULL, pixel_data_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) { perror("mmap"); return 1; }

    // Fill with a 2x2 RGBA pattern: red, green, blue, white
    uint8_t data[] = {
        0xFF,0x00,0x00,0xFF, // red
        0x00,0xFF,0x00,0xFF, // green
        0x00,0x00,0xFF,0xFF, // blue
        0xFF,0xFF,0xFF,0xFF  // white
    };
    memcpy(ptr, data, pixel_data_size);
    munmap(ptr, pixel_data_size);
    close(fd);
    // Keep SHM alive (do NOT unlink yet)

    // Now send the escape sequence
    std::string raw_name = shm_name + 1; // strip leading '/'
    std::string b64_name = base64_encode(raw_name);
    std::string escape = "\x1b_Ga=T,f=32,s=" + std::to_string(width) +
                         ",v=" + std::to_string(height) +
                         ",t=s,O=0,S=" + std::to_string(pixel_data_size) +
                         ";" + b64_name + "\x1b\\";
    // write(STDOUT_FILENO, escape.c_str(), escape.size());

    // Wait for Kitty to read (optional)
    usleep(100000);
    // Cleanup
    shm_unlink(shm_name);
    return 0;
}