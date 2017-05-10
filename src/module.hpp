/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <error.hpp>
#include <ir.hpp>

error_state ImportModule(const char* modulePath, parse_result& parse);
error_state ExportModule(const char* outputPath, parse_result& parse);
