// Copyright (C) 2019 Luxoft Sweden AB
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "daemon/backends/fake_backend.h"

#include <string>
#include <utility>
#include <vector>

namespace ConnectivityManager::Daemon
{
    FakeBackend::FakeBackend()
    {
        wifi_enable();
    }

    FakeBackend::~FakeBackend() = default;

    void FakeBackend::wifi_enable()
    {
        wifi_status_set(WiFiStatus::ENABLED);

        std::vector<WiFiAccessPoint> aps;
        for (int i = 0; i < 10; i++) {
            WiFiAccessPoint &ap = aps.emplace_back();
            ap.id = wifi_access_point_next_id();
            ap.ssid = "Test " + std::to_string(i + 1);
            ap.strength = 100 - i;
            ap.connected = false;
        }

        wifi_access_points_add_all(std::move(aps));
    }

    void FakeBackend::wifi_disable()
    {
        wifi_access_points_remove_all();
        wifi_status_set(WiFiStatus::DISABLED);
    }

    void FakeBackend::wifi_connect(const WiFiAccessPoint &access_point,
                                   ConnectFinished &&finished,
                                   RequestCredentialsFromUser && /*request_credentials*/)
    {
        if (access_point.connected) {
            finished(ConnectResult::SUCCESS);
            return;
        }

        constexpr unsigned int DELAY_SECONDS = 5;

        Glib::signal_timeout().connect_seconds(
            [this, id = access_point.id, finished = std::move(finished)] {
                for (auto &[id, ap] : state().wifi.access_points)
                    wifi_disconnect(ap);

                if (auto ap = wifi_access_point_find(id); ap) {
                    wifi_access_point_connected_set(*ap, true);
                    finished(ConnectResult::SUCCESS);
                } else {
                    finished(ConnectResult::FAILED);
                }

                return false; // No repeat.
            },
            DELAY_SECONDS);
    }

    void FakeBackend::wifi_disconnect(const WiFiAccessPoint &access_point)
    {
        if (auto ap = wifi_access_point_find(access_point.id); ap)
            wifi_access_point_connected_set(*ap, false);
    }

    void FakeBackend::wifi_hotspot_enable()
    {
        for (auto &[id, ap] : state().wifi.access_points)
            wifi_disconnect(ap);

        wifi_hotspot_status_set(WiFiHotspotStatus::ENABLED);
    }

    void FakeBackend::wifi_hotspot_disable()
    {
        wifi_hotspot_status_set(WiFiHotspotStatus::DISABLED);
    }

    void FakeBackend::wifi_hotspot_change_ssid(const std::string &ssid)
    {
        wifi_hotspot_ssid_set(ssid);
    }

    void FakeBackend::wifi_hotspot_change_passphrase(const Glib::ustring &passphrase)
    {
        wifi_hotspot_passphrase_set(passphrase);
    }
}
