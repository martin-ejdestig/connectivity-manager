// Copyright (C) 2019 Luxoft Sweden AB
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#ifndef CONNECTIVITY_MANAGER_DAEMON_ARGUMENTS_H
#define CONNECTIVITY_MANAGER_DAEMON_ARGUMENTS_H

#include <optional>
#include <ostream>

namespace ConnectivityManager::Daemon
{
    struct Arguments
    {
        static std::optional<Arguments> parse(int argc, char *argv[], std::ostream &output);

        bool print_version_and_exit = false;
    };
}

#endif // CONNECTIVITY_MANAGER_DAEMON_ARGUMENTS_H
