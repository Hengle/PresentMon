// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Params.h"


namespace p2c::infra::svc
{
    Params& Params::At(const std::string& key)
    {
        return Get<Params>(key);
    }

    const Params& Params::At(const std::string& key) const
    {
        return Get<Params>(key);
    }

    bool Params::IsEmpty() const noexcept
    {
        return container.empty();
    }

    Params::Params(std::initializer_list<ContainerType::value_type> elements)
        :
        container{ elements }
    {}

    std::any& Params::GetAny(const std::string& key)
    {
        try {
            return container.at(key);
        }
        catch (const std::out_of_range& e) {
            p2clog.note(std::format(L"Params::Get unavailable key: {}", util::ToWide(key))).ex(WrongType{}).nested(e).commit();
            throw;
        }
    }

    const std::any& Params::GetAny(const std::string& key) const
    {
        return const_cast<Params*>(this)->GetAny(key);
    }
}