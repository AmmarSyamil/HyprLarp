#include <iostream>
#include <string>
#include "base64converter.hpp"


int main() {
    std::string test = "HyprLarp:0:0";

    std::cout << base64Converter(test) << std::endl;

    return 0;
}