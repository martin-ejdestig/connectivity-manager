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

            if (i == 1) {
                ap.ssid += " - Will ask for pwd";
                request_pwd_info_.ids.insert(ap.id);
            }

            if (i == 4 || i == 6) {
                ap.ssid += " - Strength will change";
                strength_change_info_.ids.push_back(ap.id);
            }

            if (i == 5) {
                ap.ssid += " - SSID will change";
                ssid_change_info_.ids.push_back(ap.id);
            }
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

        constexpr unsigned int STRENGTH_CHANGE_DELAY_SECONDS = 4;
        strength_change_info_.timer_connection = Glib::signal_timeout().connect_seconds(
            [&] {
                for (auto id : strength_change_info_.ids) {
                    if (auto ap = wifi_access_point_find(id); ap) {
                        wifi_access_point_strength_set(*ap,
                                                       ap->strength > 10 ? ap->strength - 1 : 100);
                        g_message("Changing strength on \"%s\": %d",
                                  ap->ssid.c_str(),
                                  int(ap->strength));
                    }
                }
                return true;
            },
            STRENGTH_CHANGE_DELAY_SECONDS);

        constexpr unsigned int SSID_CHANGE_DELAY_SECONDS = 4;
        ssid_change_info_.timer_connection = Glib::signal_timeout().connect_seconds(
            [&] {
                for (auto id : ssid_change_info_.ids) {
                    if (auto ap = wifi_access_point_find(id); ap) {
                        std::string new_ssid = ap->ssid;
                        if (new_ssid.back() < '0' || new_ssid.back() > '9')
                            new_ssid += " 0";
                        new_ssid.back() += 1;
                        if (new_ssid.back() > '9')
                            new_ssid.back() = '0';
                        g_message("Changing SSID on \"%s\" to \"%s\"",
                                  ap->ssid.c_str(),
                                  new_ssid.c_str());
                        wifi_access_point_ssid_set(*ap, new_ssid);
                    }
                }
                return true;
            },
            SSID_CHANGE_DELAY_SECONDS);
    }

    void FakeBackend::wifi_disable()
    {
        if (pending_connect_.finished) {
            pending_connect_.finished(ConnectResult::FAILED);
            pending_connect_.delay_timeout_connection.disconnect();
            pending_connect_ = PendingConnect();
        }

        stay_or_go_ap_info_.timer_connection.disconnect();

        strength_change_info_.ids = {};
        strength_change_info_.timer_connection.disconnect();

        ssid_change_info_.ids = {};
        ssid_change_info_.timer_connection.disconnect();

        wifi_hotspot_status_set(WiFiHotspotStatus::DISABLED);
        hotspot_info_.disable_reconnect_ap_id = WiFiAccessPoint::ID_EMPTY;

        request_pwd_info_.ids.clear();

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

        if (request_pwd_info_.ids.find(access_point.id) != request_pwd_info_.ids.cend()) {
            using Requested = Common::Credentials::Requested;
            using Password = Common::Credentials::Password;

            Requested requested;

            requested.description_type = Requested::TYPE_WIRELESS_NETWORK;
            requested.description_id = access_point.ssid;
            requested.credentials.password = Password{Password::Type::WPA_PSK, ""};

            pending_connect_.request_credentials(
                requested, [&](auto result) { request_credentials_reply(result); });

            return;
        }

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
        for (auto &[id, ap] : state().wifi.access_points) {
            if (ap.connected) {
                hotspot_info_.disable_reconnect_ap_id = id;
                wifi_disconnect(ap);
            }
        }

        wifi_hotspot_status_set(WiFiHotspotStatus::ENABLED);
    }

    void FakeBackend::wifi_hotspot_disable()
    {
        wifi_hotspot_status_set(WiFiHotspotStatus::DISABLED);

        auto reconnect_ap_id = hotspot_info_.disable_reconnect_ap_id;
        hotspot_info_.disable_reconnect_ap_id = WiFiAccessPoint::ID_EMPTY;

        if (reconnect_ap_id != WiFiAccessPoint::ID_EMPTY)
            if (auto ap = wifi_access_point_find(reconnect_ap_id); ap)
                wifi_access_point_connected_set(*ap, true);
    }

    void FakeBackend::wifi_hotspot_change_ssid(const std::string &ssid)
    {
        wifi_hotspot_ssid_set(ssid);
    }

    void FakeBackend::wifi_hotspot_change_passphrase(const Glib::ustring &passphrase)
    {
        wifi_hotspot_passphrase_set(passphrase);
    }

    void FakeBackend::request_credentials_reply(const std::optional<Common::Credentials> &result)
    {
        using Password = Common::Credentials::Password;

        auto connect_ap = wifi_access_point_find(pending_connect_.ap_id);
        if (!connect_ap)
            return;

        auto finished = std::move(pending_connect_.finished);
        pending_connect_ = PendingConnect(); // Prevent recursion since not async when fake... ugly,
                                             // but who cares... it's all fake!

        ConnectResult connect_result = ConnectResult::FAILED;

        if (result && result->password) {
            if (result->password->type == Password::Type::WPA_PSK &&
                result->password->value == "123") {
                connect_result = ConnectResult::SUCCESS;
                g_message("Woho, correct password type and value in reply, connect success");
            } else {
                g_message("Wrong password, WPA PSK with value 123 expected (value = %s)",
                          result->password->value.c_str());
            }
        } else {
            if (!result)
                g_message("No pwd request result...?");
            else
                g_message("No pwd set in pwd result");
        }

        if (connect_result == ConnectResult::SUCCESS) {
            for (auto &[id, ap] : state().wifi.access_points)
                wifi_disconnect(ap);

            wifi_access_point_connected_set(*connect_ap, true);
        }

        finished(connect_result);
    }
}
