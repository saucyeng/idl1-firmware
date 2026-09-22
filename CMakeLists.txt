# Top-level CMakeLists for the IDL1 firmware.
# Standard ESP-IDF project layout; see §4 of IDL0_SPEC.md (spec location: docs/README.md).

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# project name is load-bearing: the OTA board guard (main/ota_guard.c,
# decisions.md F5) rejects any image whose esp_app_desc_t.project_name differs.
project(idl1_firmware)
