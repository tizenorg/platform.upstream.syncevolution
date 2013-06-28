/*
 * Copyright (C) 2011 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "NotificationBackendNoop.h"

SE_BEGIN_CXX

NotificationBackendNoop::NotificationBackendNoop()
{
}

NotificationBackendNoop::~NotificationBackendNoop()
{
}

bool NotificationBackendNoop::init()
{
    return true;
}

void NotificationBackendNoop::publish(
    const std::string& summary, const std::string& body,
    const std::string& viewParams)
{
}

SE_END_CXX

