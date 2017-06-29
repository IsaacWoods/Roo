/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <error.hpp>
#include <ir.hpp>

ErrorState ImportModule(const char* modulePath, ParseResult& parse);
ErrorState ExportModule(const char* outputPath, ParseResult& parse);
