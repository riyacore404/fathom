#include "lobster_parser.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
    // splits a line into fields, throws with line context if the field count is wrong
    std::vector<std::string> split_and_validate(const std::string& line, size_t expected_fields, size_t line_num) {
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        if (fields.size() != expected_fields) {
            throw std::runtime_error(
                "malformed row at line " + std::to_string(line_num) +
                ": expected " + std::to_string(expected_fields) +
                " fields, got " + std::to_string(fields.size()) +
                " (raw line: \"" + line + "\")");
        }
        return fields;
    }

    int64_t parse_int_field(const std::string& s, size_t line_num, const std::string& field_name) {
        try {
            return std::stoll(s);
        } catch (const std::exception&) {
            throw std::runtime_error(
                "malformed " + field_name + " at line " + std::to_string(line_num) +
                ": could not parse \"" + s + "\" as an integer");
        }
    }

    double parse_double_field(const std::string& s, size_t line_num, const std::string& field_name) {
        try {
            return std::stod(s);
        } catch (const std::exception&) {
            throw std::runtime_error(
                "malformed " + field_name + " at line " + std::to_string(line_num) +
                ": could not parse \"" + s + "\" as a number");
        }
    }
}

std::vector<LobsterMessage> parse_lobster_messages(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file: " + path);
    }

    std::vector<LobsterMessage> messages;
    std::string line;
    size_t line_num = 0;

    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;  // tolerate trailing blank lines

        auto fields = split_and_validate(line, 6, line_num);

        LobsterMessage msg;
        msg.time      = parse_double_field(fields[0], line_num, "time");
        msg.type      = static_cast<int>(parse_int_field(fields[1], line_num, "type"));
        msg.order_id  = parse_int_field(fields[2], line_num, "order_id");
        msg.size      = parse_int_field(fields[3], line_num, "size");
        msg.price     = parse_int_field(fields[4], line_num, "price");
        msg.direction = static_cast<int>(parse_int_field(fields[5], line_num, "direction"));

        if (msg.type != 1 && msg.type != 2 && msg.type != 3 &&
            msg.type != 4 && msg.type != 5 && msg.type != 7) {
            throw std::runtime_error(
                "unrecognized message type " + std::to_string(msg.type) +
                " at line " + std::to_string(line_num) +
                " (expected 1, 2, 3, 4, 5, or 7)");
        }

        messages.push_back(msg);
    }

    return messages;
}

std::vector<LobsterBookSnapshot> parse_lobster_orderbook(const std::string& path, int num_levels) {
    if (num_levels <= 0) {
        throw std::invalid_argument("num_levels must be positive, got " + std::to_string(num_levels));
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("could not open file: " + path);
    }

    std::vector<LobsterBookSnapshot> snapshots;
    std::string line;
    size_t line_num = 0;
    size_t expected_fields = static_cast<size_t>(num_levels) * 4;

    while (std::getline(file, line)) {
        line_num++;
        if (line.empty()) continue;

        auto fields = split_and_validate(line, expected_fields, line_num);

        LobsterBookSnapshot snap;
        for (int lvl = 0; lvl < num_levels; ++lvl) {
            size_t base = lvl * 4;
            snap.ask_prices.push_back(parse_int_field(fields[base + 0], line_num, "ask_price"));
            snap.ask_sizes.push_back(parse_int_field(fields[base + 1], line_num, "ask_size"));
            snap.bid_prices.push_back(parse_int_field(fields[base + 2], line_num, "bid_price"));
            snap.bid_sizes.push_back(parse_int_field(fields[base + 3], line_num, "bid_size"));
        }

        snapshots.push_back(snap);
    }

    return snapshots;
}