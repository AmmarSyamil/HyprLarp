// This file contain the implementation of converting raw list of [at,pos] data of each window to a layout that can easily be accesed in the form of coordinate cartesius.

#include <cstring>
#include <unistd.h>
#include <netinet/in.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <poll.h>


#include "DataType.hpp"


// Function to convert [at, size] into cartesius coordinate [x, y]
WindowPosCartesian ConvertPosFormat(WindowPos& DataInput) {
    // Convert to 4  point for each corver of the window by format of cartesius coordinate

    //Point of left up
    // std::vector<int> LeftUp{DataInput[0][0],DataInput[0][1]}; // (at x, at y)
    // std::vector<int> LeftDown{DataInput[0][0], DataInput[0][1] - DataInput[1][1]}; // (at x, at y - size y)
    // std::vector<int> RightUp{DataInput[0][0] + DataInput[1][0], DataInput[0][1]}; // (at x + size x, at y)
    // std::vector<int> RightDown{DataInput[0][0] + DataInput[1][0], DataInput[0][1] - DataInput[1][1]}; // (at x + size x, at y - size y)

    // std::cout << "LeftUp:    (" << LeftUp[0] << ", " << LeftUp[1] << ")" << std::endl;
    // std::cout << "LeftDown:  (" << LeftDown[0] << ", " << LeftDown[1] << ")" << std::endl;
    // std::cout << "RightUp:   (" << RightUp[0] << ", " << RightUp[1] << ")" << std::endl;
    // std::cout << "RightDown: (" << RightDown[0] << ", " << RightDown[1] << ")" << std::endl;

    // New implementation

    WindowPosCartesian output;

    output.topLeft = {DataInput.at[0], DataInput.at[1]};
    output.topRight = {DataInput.at[0], DataInput.at[1] + DataInput.size[1]};
    output.bottomLeft = {DataInput.at[0]+DataInput.size[0], DataInput.at[1]};
    output.bottomRight = {DataInput.at[0] + DataInput.size[0], DataInput.at[1] + DataInput.size[1]};


    return output;
}


// function to show all of the layout of the windows
// Constraint : The vector vector is jsut one data, i need vector vector vector of data to work 
// Which mean I need to convert all single data from socketsend.cpp into batcch in a workspaee tp cpnvert it up here

// int Layout(std::vector<std::vector<int>>& DataInput) {
//     // DataInput form = [at, size] = [[x, y], [x, y]]


//     return 1;
// }


// Testing grounds
// int main() {
//     // We need to get testing daata dummy
//     std::cout << "test part pre populate test data" << std::endl;

//     std::vector<std::vector<int>> Data {
//         {81,31},
//         {1808, 1018}
//     };

//     std::cout << "Test part post populate test data" << std::endl;



//     int test = ConvertPosFormat(Data);
// }