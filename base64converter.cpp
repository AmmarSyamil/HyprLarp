#include <iostream>
#include <string>


static const char table[] ="ABCDEFGHIJKLMNOPQRSTUVWXYZ""abcdefghijklmnopqrstuvwxyz""0123456789+/";

std::string base64Converter(const std::string& input) {
    std::string out;
    int val = 0;
    int valb = -6;

    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;

        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        out.push_back(table[((val << 8) >> (valb + 8) &0x3F)]);
    }

    while (out.size() % 4)
        out.push_back('=');

    return out;
}