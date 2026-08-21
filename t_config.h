
#ifndef SHS_ENHANCED_T_CONFIG_H
#define SHS_ENHANCED_T_CONFIG_H

#include <map>
#include <vector>
#include <fstream>

#define SECTION_MAIN "[main]"
#define CARD_ID "id"
#define CARD_ADDRESS "address"
#define CARD_INPUTS "inputs"
#define CARD_OUTPUTS "outputs"
#define CARD_DISABLED "disabled"
#define CARD_INPUT_TYPES "input_types"
#define CARD_INPUT_BROKEN "input_broken"
#define CARD_OUTPUT_BROKEN "output_broken"
#define SENSOR_OPTION_ID "id"
#define SENSOR_OPTION_NAME "name"
#define SENSOR_OPTION_ADDRESS "address"
#define SENSOR_OPTION_DISABLED "disabled"
#define METRIC_OPTION_HELP "help"
#define METRIC_OPTION_TYPE "type"
#define METRIC_OPTION_NAME "metric"

struct t_in {
    int index;
    int address;
    bool broken;
    std::string type;
};
struct t_out {
    int index;
    int address;
    bool broken;
};
struct t_card
{
    int id;
    int address;
    bool disabled;
    std::vector<t_in> in;
    std::vector<t_out> out;
};
struct t_metric {
    std::string name;
    std::string help;
    std::string type;
};
struct t_sensor {
    int id = 0;
    std::string name;
    int address = 0;
    bool disabled = false;
    std::vector<t_metric> metric;
};

class t_config
{
    public:
    t_config();
    ~t_config();

    std::vector<t_card> card;
    std::vector<t_sensor> sensor;
    std::map<std::string,std::string> options;

    void reload();
    void parse(const std::string& file);

    private:
    std::ifstream _file;
    std::string _config_file;
    static std::string _trim(std::string& str);
    static std::string _trim_left(std::string& str);
    static std::string _trim_right(std::string& str);
    static bool _try_parse_int(const std::string& str, int& out);
    static bool _try_parse_int(char ch, int& out);
    static int _id(const std::string& key,const std::string& value);
    static std::string _sensor_name(const std::string& key,std::string& value);
    static int _sensor_address(const std::string& key,const std::string& value);
    static bool _sensor_disabled(const std::string& key,const std::string& value);
    static std::string _metric_value(const std::string& key,std::string& value);
    static void _remove_signs(std::string& str);
};

#endif //SHS_ENHANCED_T_CONFIG_H
