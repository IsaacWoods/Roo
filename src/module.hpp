/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <error.hpp>
#include <ir.hpp>

/*
 * Loads the types, functions and operators from the ELF module file into the parse_result.
 * Usually executed for every imported module.
 */
error_state ImportModule(parse_result& parse, const char* modulePath);
