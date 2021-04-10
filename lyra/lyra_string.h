// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0
// See LICENSE.txt for license information
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"

struct lyra_string {
    uint32_t len;
    char data[];
};

struct lyra_string *lyra_string_from_const(struct lyra_ctx *ctx,
                                           const char *data, size_t len);

struct lyra_string *lyra_string_add(struct lyra_ctx *ctx,
                                    const struct lyra_string *left,
                                    const struct lyra_string *right);