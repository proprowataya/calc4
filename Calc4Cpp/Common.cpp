#include "Common.h"

std::vector<std::string> Split(const std::string &str, char c) {
    std::string::size_type begin = 0;
    std::vector<std::string> vec;

    while (begin < str.length()) {
        std::string::size_type end = str.find_first_of(c, begin);
        vec.push_back(str.substr(begin, end - begin));

        if (end == std::string::npos) {
            break;
        }

        begin = end + 1;
    }

    return vec;
}

std::string TrimWhiteSpaces(const std::string &str) {
    std::string::size_type left = str.find_first_not_of(' ');

    if (left == std::string::npos) {
        return str;
    }

    std::string::size_type right = str.find_last_not_of(' ');
    return str.substr(left, right - left + 1);
}
