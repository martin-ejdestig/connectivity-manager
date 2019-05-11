// Copyright (C) 2019 Luxoft Sweden AB
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// SPDX-License-Identifier: MPL-2.0

#ifndef CONNECTIVITY_MANAGER_COMMON_VARIANT_GET_H
#define CONNECTIVITY_MANAGER_COMMON_VARIANT_GET_H

#include <glibmm.h>

#include <optional>
#include <typeinfo>

namespace ConnectivityManager::Common
{
    // TODO: Send a patch to glibmm that adds a method to Glib::VariantBase that behaves like this?
    template <typename T>
    std::optional<T> variant_get(const Glib::VariantBase &variant)
    {
        T value;

        try {
            value = Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(variant).get();
        } catch (const std::bad_cast &) {
            return {};
        }

        return value;
    }
}

#endif // CONNECTIVITY_MANAGER_COMMON_VARIANT_GET_H
