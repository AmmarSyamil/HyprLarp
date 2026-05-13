// This file contain the implementation of converting raw list of [at,pos] data of each window to a layout that can easily be accesed in the form of coordinate cartesius.

#include <unistd.h>
#include <netinet/in.h>
#include <fstream>
#include <iostream>
#include <vector>

// Function to convert epstein into cartesius coordinate
int ConvertPosFormat(std::vector<std::vector<int>>& DataInput) {
    // Convert to 4  point for each corver of the window by format of cartesius coordinate

    //Point of left up

    std::vector<int> LeftUp = DataInput[1]; // (at x, at y)
    std::vector<int> LeftDown{}; // (at x, at y - size y)
    std::vector<int> RightUp{}; // (at x + size x, at y)
    std::vector<int> RightDown{}; // (at x + size x, at y - size y)

    return 1;
}


// Main function to show all of the layout of the windows
int Main(std::vector<std::vector<int>>& DataInput) {
    // DataInput form = [at, size] = [[x, y], [x, y]]

    return 1;
}