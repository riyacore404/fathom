#pragma once
#include <string>

struct Config {
    std::string ticker = "AMZN";
    std::string date = "2012-06-21";
    std::string start_time = "34200000";
    std::string end_time = "57600000";
    int level = 10;
    std::string data_dir = "data";
    bool log_impact_data = false;         
    int impact_horizon_messages = 20;    
};

std::string message_file_path(const Config& cfg);