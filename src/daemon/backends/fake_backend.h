// Copyright (C) 2019 Luxoft Sweden AB
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#ifndef CONNECTIVITY_MANAGER_DAEMON_BACKENDS_FAKE_BACKEND_H
#define CONNECTIVITY_MANAGER_DAEMON_BACKENDS_FAKE_BACKEND_H

#include "daemon/backend.h"

namespace ConnectivityManager::Daemon
{
    class FakeBackend final : public Backend
    {
    public:
        FakeBackend();
        ~FakeBackend() final;

        void wifi_enable() override;
        void wifi_disable() override;
        void wifi_connect(const WiFiAccessPoint &access_point,
                          ConnectFinished &&finished,
                          RequestCredentialsFromUser &&request_credentials) override;
        void wifi_disconnect(const WiFiAccessPoint &access_point) override;

        void wifi_hotspot_enable() override;
        void wifi_hotspot_disable() override;
        void wifi_hotspot_change_ssid(const std::string &ssid) override;
        void wifi_hotspot_change_passphrase(const Glib::ustring &passphrase) override;
    };
}

#endif // CONNECTIVITY_MANAGER_DAEMON_BACKENDS_FAKE_BACKEND_H