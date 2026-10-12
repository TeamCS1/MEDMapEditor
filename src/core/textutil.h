#pragma once
#include <string>
#include <vector>

namespace med {

// Trim spaces, tabs and CR/LF from both ends.
std::string trim(const std::string& s);
// Split on ',' and trim each field (IDE/IPL lines are comma separated).
std::vector<std::string> splitFields(const std::string& line);
// True for blank lines and '#' comments.
bool isBlankOrComment(const std::string& trimmedLine);
std::string toLower(std::string s);

}  // namespace med
