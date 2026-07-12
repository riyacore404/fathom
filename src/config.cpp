#include "config.hpp"

std::string message_file_path(const Config& cfg) {
    return cfg.data_dir + "/" + cfg.ticker + "_" + cfg.date + "_" +
           cfg.start_time + "_" + cfg.end_time + "_message_" + std::to_string(cfg.level) + ".csv";
}