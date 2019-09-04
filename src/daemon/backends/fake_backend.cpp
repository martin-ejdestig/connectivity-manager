// Copyright (C) 2019 Luxoft Sweden AB
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#include "daemon/backends/fake_backend.h"

#include <glibmm.h>

#include <string>
#include <utility>
#include <vector>

// TODO:
// - Periodically change strength on a couple of APs.
// - Periodically change SSID on a couple of APs... or maybe just one.
// - Reconnect previously connected ap if hotspot is disabled? Maybe punt for now.
// - Story for connecting global state. What is in CM and NM?
// - Higher prio on stories for more info in AP ifc? (connected/connecting, security).
// - Ask for password on at least one AP.

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

        constexpr unsigned int STAY_OR_GO_DELAY_SECONDS = 10;
        stay_or_go_ap_info_.timer_connection = Glib::signal_timeout().connect_seconds(
            [&] {
                if (stay_or_go_ap_info_.id == WiFiAccessPoint::ID_EMPTY) {
                    WiFiAccessPoint ap;
                    ap.id = wifi_access_point_next_id();
                    ap.ssid = "Should I stay or should I go";
                    ap.strength = 50;
                    ap.connected = false;
                    stay_or_go_ap_info_.id = ap.id;
                    g_message("Adding AP \"%s\"", ap.ssid.c_str());
                    wifi_access_point_add(std::move(ap));
                } else if (auto ap = wifi_access_point_find(stay_or_go_ap_info_.id); ap) {
                    if (pending_connect_.ap_id == ap->id) {
                        pending_connect_.finished(ConnectResult::FAILED);
                        pending_connect_.delay_timeout_connection.disconnect();
                        pending_connect_ = PendingConnect();
                    }
                    stay_or_go_ap_info_.id = WiFiAccessPoint::ID_EMPTY;
                    g_message("Removing AP \"%s\"", ap->ssid.c_str());
                    wifi_access_point_remove(*ap);
                }
                return true;
            },
            STAY_OR_GO_DELAY_SECONDS);
    }

    void FakeBackend::wifi_disable()
    {
        stay_or_go_ap_info_.timer_connection.disconnect();

        wifi_access_points_remove_all();

        wifi_status_set(WiFiStatus::DISABLED);
    }

    void FakeBackend::wifi_connect(const WiFiAccessPoint &access_point,
                                   ConnectFinished &&finished,
                                   RequestCredentialsFromUser &&request_credentials)
    {
        if (access_point.connected) {
            finished(ConnectResult::SUCCESS);
            return;
        }

        if (pending_connect_.finished) {
            pending_connect_.finished(ConnectResult::FAILED);
            pending_connect_.delay_timeout_connection.disconnect();
            pending_connect_ = PendingConnect();
        }

        pending_connect_.ap_id = access_point.id;
        pending_connect_.finished = std::move(finished);
        pending_connect_.request_credentials = std::move(request_credentials);

        constexpr unsigned int DELAY_SECONDS = 5;

        pending_connect_.delay_timeout_connection = Glib::signal_timeout().connect_seconds(
            [&] {
                for (auto &[id, ap] : state().wifi.access_points)
                    wifi_disconnect(ap);

                if (auto ap = wifi_access_point_find(pending_connect_.ap_id); ap) {
                    wifi_access_point_connected_set(*ap, true);
                    pending_connect_.finished(ConnectResult::SUCCESS);
                } else {
                    pending_connect_.finished(ConnectResult::FAILED);
                }

                pending_connect_ = PendingConnect();

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
