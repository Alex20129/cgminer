cmake_minimum_required(VERSION 3.0)

add_library(driver-s17 "driver-btm-soc.cpp" "driver-btm-soc.hpp"
                         "eeprom.cpp" "eeprom.hpp"
                         "power.cpp" "power.hpp"
                         "util.cpp" "util.hpp"
                         "logging.cpp" "logging.hpp")
