#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "impact_logger.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>

// splits a CSV row into fields -- shared test helper, keeps assertions robust
// against exact formatting instead of doing fragile substring matches
static std::vector<std::string> split_csv_row(const std::string& row) {
    std::vector<std::string> fields;
    std::stringstream ss(row);
    std::string field;
    while (std::getline(ss, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

TEST_CASE("log_trade_impact_features writes correct feature/label rows for a simple synthetic sequence") {
    std::vector<LobsterMessage> messages = {
        {1.0, 1, 1, 100, 10000, 1},   // resting buy at 10000
        {1.0, 1, 2, 100, 10010, -1},  // resting sell at 10010
        {2.0, 4, 1, 50, 10000, 1},    // execution: resting buy order 1 gets hit (50 shares)
        {3.0, 1, 3, 100, 10020, -1},  // new resting sell further out -- moves mid up
    };

    std::string path = "test_impact_output.csv";
    size_t written = log_trade_impact_features(messages, 10, /*horizon_messages=*/1, path);

    REQUIRE(written == 1);

    std::ifstream in(path);
    std::string header, row;
    std::getline(in, header);
    std::getline(in, row);

    auto fields = split_csv_row(row);
    // columns: time,message_index,trade_qty,direction,best_bid,best_ask,spread,
    //          bid_depth_top3,ask_depth_top3,imbalance,pre_trade_mid,label_price_move
    REQUIRE(fields.size() == 12);
    REQUIRE(std::stod(fields[0]) == Catch::Approx(2.0));   // time
    REQUIRE(std::stoi(fields[1]) == 2);                     // message_index
    REQUIRE(std::stoll(fields[2]) == 50);                   // trade_qty
    REQUIRE(std::stoi(fields[3]) == 1);                     // direction
    REQUIRE(std::stoll(fields[4]) == 10000);                // best_bid
    REQUIRE(std::stoll(fields[5]) == 10010);                // best_ask
    REQUIRE(std::stod(fields[10]) == Catch::Approx(10005.0)); // pre_trade_mid

    std::remove(path.c_str());
}

TEST_CASE("rows too close to the end of the data are correctly skipped") {
    std::vector<LobsterMessage> messages = {
        {1.0, 1, 1, 100, 10000, 1},
        {1.0, 1, 2, 100, 10010, -1},
        {2.0, 4, 1, 50, 10000, 1},   // execution -- but horizon of 5 messages doesn't exist
    };

    std::string path = "test_impact_skip.csv";
    size_t written = log_trade_impact_features(messages, 10, /*horizon_messages=*/5, path);

    REQUIRE(written == 0);

    std::remove(path.c_str());
}