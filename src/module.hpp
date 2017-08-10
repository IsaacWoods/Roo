/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <error.hpp>
#include <ir.hpp>

ErrorState* ImportModule(const std::string& modulePath, ParseResult& parse);
ErrorState* ExportModule(const std::string& outputPath, ParseResult& parse);
