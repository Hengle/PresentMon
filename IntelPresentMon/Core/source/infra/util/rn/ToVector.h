// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once 
#include <ranges> 

namespace p2c::infra::util::rn
{
    struct ToVectorAdapter_
    {
        struct Closure_
        {
            template<std::ranges::range R>
            constexpr auto operator()(R&& r) const
            {
                auto r_common = r | std::views::common;
                std::vector<std::ranges::range_value_t<R>> v;

                if constexpr (requires { std::ranges::size(r); }) {
                    v.reserve(std::ranges::size(r));
                }

                v.insert(v.begin(), r_common.begin(), r_common.end());

                return v;
            }
        };

        constexpr auto operator()() const -> Closure_
        {
            return Closure_{};
        }

        template<std::ranges::range R>
        constexpr auto operator()(R&& r)
        {
            return Closure_{}(r);
        }
    };

    inline ToVectorAdapter_ ToVector;

    template<std::ranges::range R>
    constexpr auto operator|(R&& r, ToVectorAdapter_::Closure_ const& a)
    {
        return a(std::forward<R>(r));
    }
}