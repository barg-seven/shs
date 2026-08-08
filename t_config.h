
#ifndef SHS_T_CONFIG_H
#define SHS_T_CONFIG_H

#include <map>
#include <vector>
#include <fstream>

struct t_card;

class t_config
{
public:
    t_config();
    ~t_config();

    std::vector<t_card> card;
    std::map<std::string,std::string> options;

    void reload();
    void parse(const std::string& file);

private:
    std::ifstream _file;
    std::string _config_file;
    static std::string _trim(std::string& str);
    static bool _try_parse_int(const std::string& str, int& out);
    static bool _try_parse_int(char ch, int& out);
};

struct t_card
{
    int index;
    int address;
    struct t_in {
        int index;
        int address;
        std::string type;
    };
    std::vector<t_in> in;
};

#endif //SHS_T_CONFIG_H
