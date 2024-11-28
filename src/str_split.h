#ifndef str_split_h
#define str_split_h

#include <Arduino.h>
#include <vector>

void str_split(std::vector<String>& vector, const char* str, size_t len, const char delimiter)
{
    String sub;
    for (size_t i = 0; i < len; i++) {
        if (str[i] != delimiter)
            sub += str[i];
        else {
            vector.push_back(sub);
            sub.clear();
        }
    }
    vector.push_back(sub);
}

void str_split(std::vector<String>& vector, const String& str, const char delimiter)
{
    size_t len = str.length();
    str_split(vector, str.c_str(), len, delimiter);
}

#endif
