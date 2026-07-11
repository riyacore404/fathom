#include "lobster_parser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

std::vector<LobsterMessage> parse_lobster_messages(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file: " + path);
    }

    std::vector<LobsterMessage> messages;
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string field;
        LobsterMessage msg;

        std::getline(ss, field, ','); msg.time      = std::stod(field);
        std::getline(ss, field, ','); msg.type      = std::stoi(field);
        std::getline(ss, field, ','); msg.order_id  = std::stoll(field);
        std::getline(ss, field, ','); msg.size      = std::stoll(field);
        std::getline(ss, field, ','); msg.price     = std::stoll(field);
        std::getline(ss, field, ','); msg.direction = std::stoi(field);

        messages.push_back(msg);
    }

    return messages;
}

std::vector<LobsterBookSnapshot> parse_lobster_orderbook(const std::string& path, int num_levels) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file: " + path);
    }

    std::vector<LobsterBookSnapshot> snapshots;
    std::string line;

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string field;
        LobsterBookSnapshot snap;

        for (int lvl = 0; lvl < num_levels; ++lvl) {
            std::getline(ss, field, ','); snap.ask_prices.push_back(std::stoll(field));
            std::getline(ss, field, ','); snap.ask_sizes.push_back(std::stoll(field));
            std::getline(ss, field, ','); snap.bid_prices.push_back(std::stoll(field));
            std::getline(ss, field, ','); snap.bid_sizes.push_back(std::stoll(field));
        }

        snapshots.push_back(snap);
    }

    return snapshots;
}