#include "Common.h"

char snprintfBuffer[SnprintfBufferSize];

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

// https://stackoverflow.com/questions/25114597/how-to-print-int128-in-g
std::ostream& operator<<(std::ostream& dest, __int128_t value) {
    std::ostream::sentry s(dest);

    if (s) {
        __uint128_t tmp = value < 0 ? -value : value;
        char buffer[128];
        char* d = std::end(buffer);

        do {
            --d;
            *d = "0123456789"[tmp % 10];
            tmp /= 10;
        } while (tmp != 0);
        if (value < 0) {
            --d;
            *d = '-';
        }

        int len = std::end(buffer) - d;

        if (dest.rdbuf()->sputn(d, len) != len) {
            dest.setstate(std::ios_base::badbit);
        }
    }

    return dest;
}
