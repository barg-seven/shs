//
// Created by aj on 5/29/26.
//

#ifndef SHS_T_CONFIG_H
#define SHS_T_CONFIG_H

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <charconv> // std::from_chars()


class t_config {

public:
    struct t_input {
        int index = 0;
        std::string type;
    };
    struct t_card {
        t_card() {
            input.resize(8);
        }
        int index = 0;
        int address = 0;
        std::vector<t_input> input;
    };
    struct t_waveshare {
        std::vector<t_card> card;
    };
    explicit t_config(const std::string& filename) {
        _filename = filename;
        _init_options();
    }

    ~t_config() {
        if (_file.is_open()) _file.close();
    };

    t_waveshare waveshare;
    std::map<std::string,std::string> options;

private:
    std::ifstream _file;
    std::string _filename;
    void _init_options();
    static std::string trim(std::string& str);
    static bool try_parse_int(const std::string& str, int& outValue);
    static bool try_parse_int(char ch, int& out_value);
    static std::vector<std::string> _split_by_underscore(const std::string& text);

    static std::string trim(const std::string& str) {
        const auto start = std::ranges::find_if_not(str, [](const unsigned char ch) { return std::isspace(ch); });
        const auto end = std::find_if_not(str.rbegin(), str.rend(), [](const unsigned char ch) { return std::isspace(ch); }).base();
        return (start < end) ? std::string(start, end) : "";
    }
};


#endif //SHS_T_CONFIG_H
