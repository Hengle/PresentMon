// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Services.h"
#include <format>
#include <sstream>

namespace p2c::infra::svc
{
    void Services::Clear()
    {
        std::unique_lock lock{ Get_().readWriteMutex };
        Get_().typeServiceMap.clear();
    }

    std::string Services::Dump()
    {
        std::shared_lock lock{ Get_().readWriteMutex };
        const auto& map = Get_().typeServiceMap;
        std::ostringstream out;
        out << std::format("Type Entries: ({})\n", map.size());
        for (auto&& [key, value] : map)
        {
            out << std::format("{}: ", key.name());
            std::visit([&out](auto&& opt) {
                using T = std::decay_t<decltype(opt)>;
                if constexpr (std::is_same_v<T, Singleton_>)
                {
                    const Singleton_& sing = opt;
                    out << sing.generatorOrInstance.type().name() << "\n";
                }
                else if constexpr (std::is_same_v<T, Generator_>)
                {
                    const Generator_& gen = opt;
                    out << gen.generator.type().name() << "\n";
                }
                else if constexpr (std::is_same_v<T, Multitagged_>)
                {
                    const Multitagged_& multi = opt;
                    out << std::format("Multitagged ({})\n", multi.tagged.size());
                    out << "\t[Primary]: ";
                    std::visit([&out](auto&& opt) {
                        std::string dummy = "dum";
                        using T2 = std::decay_t<decltype(opt)>;
                        if constexpr (std::is_same_v<T2, Singleton_>)
                        {
                            const Singleton_& sing = opt;
                            out << sing.generatorOrInstance.type().name() << "\n";
                        }
                        else if constexpr (std::is_same_v<T2, Generator_>)
                        {
                            const Generator_& gen = opt;
                            out << gen.generator.type().name() << "\n";
                        }
                        else
                        {
                            out << "MONOSTATE\n";
                        }
                    }, multi.primary);
                    for (auto&& [key, value] : multi.tagged)
                    {
                        out << "\t" << key << ": ";
                        std::visit([&out](auto&& opt) {
                            using T3 = std::decay_t<decltype(opt)>;
                            if constexpr (std::is_same_v<T3, Singleton_>)
                            {
                                const Singleton_& sing = opt;
                                out << sing.generatorOrInstance.type().name() << "\n";
                            }
                            else if constexpr (std::is_same_v<T3, Generator_>)
                            {
                                const Generator_& gen = opt;
                                out << gen.generator.type().name() << "\n";
                            }
                            else
                            {
                                out << "MONOSTATE!??\n";
                            }
                        }, value);
                    }
                }
                else
                {
                    out << "MONOSTATE!??\n";
                }
            }, value);
        }
        return out.str();
    }

    Services& Services::Get_()
    {
        static Services instance;
        return instance;
    }

    std::shared_ptr<void> Services::ResolveErased_(
        Entry_& entry,
        const Params& params,
        const std::string& tag,
        ErasedGeneratorHelper_* GetGenerated,
        ErasedSingletonHelper_* GetSingleton,
        bool throwForKey)
    {
        return std::visit([&](auto&& opt) -> std::shared_ptr<void> {
            using T2 = std::decay_t<decltype(opt)>;
            if constexpr (std::is_same_v<T2, Singleton_>)
            {
                if (!params.IsEmpty())
                {
                    p2clog.warn(L"Don't pass params when resolving a singleton").commit();
                }
                if (!tag.empty())
                {
                    p2clog.note(L"Tag passed in, but entry was not multitagged").commit();
                }
                Singleton_& singleton = opt;
                return GetSingleton(singleton, params);
            }
            else if constexpr (std::is_same_v<T2, Generator_>)
            {
                if (!tag.empty())
                {
                    p2clog.note(L"Tag passed in, but entry was not multitagged").commit();
                }
                Generator_& generator = opt;
                return GetGenerated(generator.generator, params);
            }
            else if constexpr (std::is_same_v<T2, Multitagged_>)
            {
                Multitagged_& multi = opt;
                if (tag.empty())
                {
                    return std::visit([&](auto&& opt) -> std::shared_ptr<void> {
                        using T3 = std::decay_t<decltype(opt)>;
                        if constexpr (std::is_same_v<T3, Singleton_>)
                        {
                            if (!params.IsEmpty())
                            {
                                p2clog.warn(L"Don't pass params when resolving a singleton").commit();
                            }
                            Singleton_& singleton = opt;
                            return GetSingleton(singleton, params);
                        }
                        else if constexpr (std::is_same_v<T3, Generator_>)
                        {
                            Generator_& generator = opt;
                            return GetGenerated(generator.generator, params);
                        }
                        else
                        {
                            if (throwForKey)
                            {
                                p2clog.note(L"Primary service type resolved to multitagged with empty primary").ex(NotFound{}).commit();
                            }
                            return {};
                        }
                    }, multi.primary);
                }
                else
                {
                    auto multiEntryIt = multi.tagged.find(tag);
                    if (multiEntryIt == multi.tagged.end())
                    {
                        if (throwForKey)
                        {
                            p2clog.note(std::format(L"Service type resolved to multitagged, but tag was not found [{}]", util::ToWide(tag)))
                                .ex(NotFound{}).commit();
                        }
                        return {};
                    }
                    return std::visit([&](auto&& opt) -> std::shared_ptr<void> {
                        using T3 = std::decay_t<decltype(opt)>;
                        if constexpr (std::is_same_v<T3, Singleton_>)
                        {
                            if (!params.IsEmpty())
                            {
                                p2clog.warn(L"Don't pass params when resolving a singleton").commit();
                            }
                            Singleton_& singleton = opt;
                            return GetSingleton(singleton, params);
                        }
                        else if constexpr (std::is_same_v<T3, Generator_>)
                        {
                            Generator_& generator = opt;
                            return GetGenerated(generator.generator, params);
                        }
                        else
                        {
                            p2clog.note(L"logic error: invalid variant state for tagged entry").commit();
                        }
                    }, multiEntryIt->second);
                }
            }
            else // monostate
            {
                p2clog.note(L"logic error: invalid variant state (monostate) for root entry").commit();
                return {};
            }
        }, entry);
    }


    Services::Singleton_::Singleton_(std::any g)
        :
        generatorOrInstance{ std::move(g) }
    {}

    Services::Singleton_::Singleton_(Singleton_&& s) noexcept
        :
        generatorOrInstance{ std::move(s.generatorOrInstance) },
        instanced{ (bool)s.instanced }
    {}

    Services::Singleton_& Services::Singleton_::operator=(Singleton_&& s) noexcept
    {
        generatorOrInstance = std::move(s.generatorOrInstance);
        instanced = (bool)s.instanced;
        return *this;
    }
}