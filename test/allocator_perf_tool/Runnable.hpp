// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (C) 2015 - 2018 Intel Corporation. */
#pragma once

class Runnable
{
public:
    virtual void run() = 0;
    virtual ~Runnable() {};
};
