#pragma once
#include <string>
#include "DataType.hpp"

std::string read_terminal_response(int timeout = 100);
bool parse_terminal_response(const std::string& resp, int& h, int& w);
bool query_terminal_internal_geometry(int& w, int& h, int& cols, int& rows);
InternalTerminalGeometry GetInternalTerminalGeometry(const WindowPos& pos);