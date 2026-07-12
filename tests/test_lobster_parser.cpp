#include <catch2/catch_test_macros.hpp>
#include "lobster_parser.hpp"
#include <fstream>
#include <cstdio>
#include "config.hpp"

TEST_CASE("parser throws a clear error on a row with too few fields") {
    const std::string path = "test_malformed.csv";
    {
        std::ofstream f(path);
        f << "34200.1,1,123,10,10000\n";  // missing the 6th field (direction)
    }

    REQUIRE_THROWS_AS(parse_lobster_messages(path), std::runtime_error);

    std::remove(path.c_str());
}

TEST_CASE("parser throws a clear error on non-numeric garbage") {
    const std::string path = "test_garbage.csv";
    {
        std::ofstream f(path);
        f << "34200.1,1,123,10,notanumber,1\n";
    }

    REQUIRE_THROWS_AS(parse_lobster_messages(path), std::runtime_error);

    std::remove(path.c_str());
}

TEST_CASE("parser throws on an unrecognized message type") {
    const std::string path = "test_badtype.csv";
    {
        std::ofstream f(path);
        f << "34200.1,99,123,10,10000,1\n";  // type 99 doesn't exist
    }

    REQUIRE_THROWS_AS(parse_lobster_messages(path), std::runtime_error);

    std::remove(path.c_str());
}

TEST_CASE("parse_lobster_orderbook rejects non-positive num_levels") {
    REQUIRE_THROWS_AS(parse_lobster_orderbook("doesnt_matter.csv", 0), std::invalid_argument);
}

TEST_CASE("message_file_path builds the exact LOBSTER filename convention") {
    Config cfg;
    cfg.ticker = "AMZN";
    cfg.date = "2012-06-21";
    cfg.level = 10;
    cfg.data_dir = "data";

    std::string expected = "data/AMZN_2012-06-21_34200000_57600000_message_10.csv";
    REQUIRE(message_file_path(cfg) == expected);
}