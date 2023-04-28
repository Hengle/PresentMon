// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <any>
#include <map>
#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <functional>
#include <memory>
#include <string_view>
#include <initializer_list>
#include <variant>
#include <type_traits>
#include <concepts>
#include <shared_mutex>
#include <Core/source/infra/log/Logging.h>
#include <Core/source/infra/util/Util.h>
#include "Exceptions.h"
#include "Params.h"


namespace p2c::con::svc
{
	template<typename T, typename S>
	concept NoparamInvokable = requires(T a)
	{
		{a()} -> std::convertible_to<std::shared_ptr<S>>;
	};
}

namespace p2c::infra::svc
{
    class Services
    {
        struct Singleton_
        {
            Singleton_() = default;
            Singleton_(std::any g);
            Singleton_(Singleton_&&) noexcept;
            Singleton_& operator=(Singleton_&&) noexcept;
            std::any generatorOrInstance;
            std::atomic<bool> instanced = false;
            std::mutex mtx;
        };
        struct Generator_
        {
            std::any generator;
        };
        struct Multitagged_
        {
            std::variant<std::monostate, Singleton_, Generator_> primary;
            std::map<std::string, std::variant<Singleton_, Generator_>> tagged;
        };
        using ErasedGeneratorHelper_ = std::shared_ptr<void>(std::any&, const Params&);
        using ErasedSingletonHelper_ = std::shared_ptr<void>(Singleton_&, const Params& params);
        using Entry_ = std::variant<std::monostate, Singleton_, Generator_, Multitagged_>;

    public:
        template<typename T>
        using Generator = std::function<std::shared_ptr<T>(const Params&)>;
        template<typename T>
        using NoparamGenerator = std::function<std::shared_ptr<T>()>;

        // general
        static void Clear();
        static std::string Dump();
        // untagged versions
        template<typename T>
        static void Bind(Generator<T> gen)
        {
            Get_().Bind_<T>(gen);
        }
        template<typename T, con::svc::NoparamInvokable<T> Invokable>
        static void Bind(Invokable&& i)
        {
            Get_().Bind_<T>([i = std::move(i)](const Params&){return i();});
        }
        template<typename T>
        static void InjectSingletonInstance(std::shared_ptr<T> inst)
        {
            Get_().InjectSingletonInstance_(std::move(inst));
        }
        template<typename T, con::svc::NoparamInvokable<T> Invokable>
        static void Singleton(Invokable&& i)
        {
            Get_().Bind_<T>([i = std::move(i)](const Params&){return i();}, true);
        }
        template<typename T>
        static void Singleton()
        {
            Get_().Bind_<T>([](const Params&){return std::make_shared<T>();}, true);
        }
        template<typename T> [[nodiscard]]
        static std::shared_ptr<T> Resolve(const Params& p = {})
        {
            return Get_().Resolve_<T>(p, "", true);
        }
        template<typename T> [[nodiscard]]
        static std::shared_ptr<T> ResolveOrNull(const Params& p = {})
        {
            return Get_().Resolve_<T>(p, "", false);
        }
        // tagged versions
        template<typename T>
        static void Bind(std::string tag, Generator<T> gen)
        {
            Get_().Bind_<T>(std::move(tag), std::move(gen));
        }
        template<typename T, con::svc::NoparamInvokable<T> Invokable>
        static void Bind(std::string tag, Invokable&& i)
        {
            Get_().Bind_<T>(std::move(tag), [i = std::move(i)](const Params&){return i(); });
        }
        template<typename T, typename S>
        static void BindTypeTagged(Generator<T> gen)
        {
            Get_().Bind_<T>(std::string(typeid(S).name()), std::move(gen));
        }
        template<typename T, typename S, con::svc::NoparamInvokable<T> Invokable>
        static void BindTypeTagged(Invokable&& i)
        {
            Get_().Bind_<T>(std::string(typeid(S).name()), [i = std::move(i)](const Params&){return i(); });
        }
        template<typename T>
        static void InjectSingletonInstance(std::string tag, std::shared_ptr<T> inst)
        {
            Get_().InjectSingletonInstance_(std::move(tag), std::move(inst));
        }
        template<typename T, con::svc::NoparamInvokable<T> Invokable>
        static void Singleton(std::string tag, Invokable&& i)
        {
            Get_().Bind_<T>(std::move(tag), [i = std::move(i)](const Params&){return i(); }, true);
        }
        template<typename T>
        static void Singleton(std::string tag)
        {
            Get_().Bind_<T>(std::move(tag), [](const Params&) {return std::make_shared<T>(); }, true);
        }
        template<typename T, typename S, con::svc::NoparamInvokable<T> Invokable>
        static void SingletonTypeTagged(Invokable&& i)
        {
            Get_().Bind_<T>(
                std::string(typeid(S).name()),
                [i = std::move(i)](const Params&){return i();},
                true
            );
        }
        template<typename T> [[nodiscard]]
        static std::shared_ptr<T> Resolve(const Params& p, const std::string& tag)
        {
            return Get_().Resolve_<T>(p, tag, true);
        }
        template<typename T> [[nodiscard]]
        static std::shared_ptr<T> ResolveOrNull(const Params& p, const std::string& tag)
        {
            return Get_().Resolve_<T>(p, tag, false);
        }

    private:
        // untagged
        template<typename T>
        void Bind_(Generator<T> gen, bool singleton = false)
        {
            std::unique_lock lock{ readWriteMutex };
            auto& opt = typeServiceMap[std::type_index{ typeid(T) }];
            if (std::holds_alternative<Multitagged_>(opt))
            {
                std::get<Multitagged_>(opt).primary = Generator_{ gen };
            }
            else
            {
                if (singleton)
                {
                    opt = Singleton_{ gen };
                }
                else
                {
                    opt = Generator_{ gen };
                }
            }
        }
        template<typename T>
        void InjectSingletonInstance_(std::shared_ptr<T> inst)
        {
            std::unique_lock lock{ readWriteMutex };
            auto& opt = typeServiceMap[std::type_index{ typeid(T) }];
            if (std::holds_alternative<Multitagged_>(opt))
            {
                std::get<Multitagged_>(opt).primary = Singleton_{ inst };
            }
            else
            {
                opt = Singleton_{ inst };
            }
        }
        // tagged
        template<typename T>
        void Bind_(std::string tag, Generator<T> gen, bool singleton = false)
        {
            std::unique_lock lock{ readWriteMutex };
            auto& entry = typeServiceMap[std::type_index{ typeid(T) }];
            std::visit([&entry, tag = std::move(tag), gen = std::move(gen), singleton](auto&& opt) {
                using T = std::decay_t<decltype(opt)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    Multitagged_ mt;
                    if (singleton)
                    {
                        mt.tagged[std::move(tag)] = Singleton_{ gen };
                    }
                    else
                    {
                        mt.tagged[std::move(tag)] = Generator_{ gen };
                    }
                    entry = std::move(mt);
                }
                else if constexpr (std::is_same_v<T, Multitagged_>)
                {
                    Multitagged_& multi = opt;
                    if (singleton)
                    {
                        multi.tagged[std::move(tag)] = Singleton_{ gen };
                    }
                    else
                    {
                        multi.tagged[std::move(tag)] = Generator_{ gen };
                    }
                }
                else // singleton OR generator needs to be transplanted into displacing multitagged
                {
                    Multitagged_ mt{ std::move(opt), {} };
                    if (singleton)
                    {
                        mt.tagged[std::move(tag)] = Singleton_{ gen };
                    }
                    else
                    {
                        mt.tagged[std::move(tag)] = Generator_{ gen };
                    }
                    entry = std::move(mt);
                }
            }, entry);
        }
        template<typename T>
        void InjectSingletonInstance_(std::string tag, std::shared_ptr<T> inst)
        {
            std::unique_lock lock{ readWriteMutex };
            auto& entry = typeServiceMap[std::type_index{ typeid(T) }];
            std::visit([&entry, tag = std::move(tag), inst = std::move(inst)](auto&& opt) {
                using T = std::decay_t<decltype(opt)>;
                if constexpr (std::is_same_v<T, std::monostate>)
                {
                    Multitagged_ mt;
                    mt.tagged[std::move(tag)] = Singleton_{ inst };
                    entry = std::move(mt);
                }
                else if constexpr (std::is_same_v<T, Multitagged_>)
                {
                    Multitagged_& multi = opt;
                    multi.tagged[std::move(tag)] = Singleton_{ inst };
                }
                else // singleton OR generator needs to be transplanted into displacing multitagged
                {
                    Multitagged_ mt{ std::move(opt), {} };
                    mt.tagged[std::move(tag)] = Singleton_{ inst };
                    entry = std::move(mt);
                }
            }, entry);
        }
        // common
        template<typename T>
        static std::shared_ptr<T> Generate_(std::any& gen, const Params& p)
        {
            try
            {
                return std::any_cast<Generator<T>>(gen)(p);
            }
            catch (const std::bad_any_cast& e)
            {
                p2clog.nested(e).note(std::format(L"Service found by type does not match tried:[{}] actual:[{}]",
                    util::ToWide(typeid(std::shared_ptr<T>).name()),
                    util::ToWide(gen.type().name())
                )).ex(WrongType{}).commit();
                return {};
            }
        }
        template<typename T>
        std::shared_ptr<T> Resolve_(const Params& params, const std::string& tag, bool throwForKey)
        {
            std::shared_lock lock{ readWriteMutex };
            auto entryIt = typeServiceMap.find(std::type_index{ typeid(T) });
            if (entryIt == typeServiceMap.end())
            {
                if (throwForKey)
                {
                    p2clog.note(std::format(L"Service type not found in service map [{}]", util::ToWide(typeid(T).name()))).ex(NotFound{}).commit();
                }
                return {};
            }

            ErasedGeneratorHelper_* GetGeneratedFn = [](std::any& gen, const Params& params) {
                return std::static_pointer_cast<void>(Generate_<T>(gen, params));
            };
            ErasedSingletonHelper_* GetSingletonFn = [](Singleton_& sing, const Params& params) {
                if (sing.instanced) {
                    auto inst = std::any_cast<std::shared_ptr<T>>(&sing.generatorOrInstance);
                    return std::static_pointer_cast<void>(*inst);
                }
                std::lock_guard lk{ sing.mtx };
                if (auto inst = std::any_cast<std::shared_ptr<T>>(&sing.generatorOrInstance))
                {
                    return std::static_pointer_cast<void>(*inst);
                }
                else
                {
                    auto instNew = Generate_<T>(sing.generatorOrInstance, params);
                    sing.generatorOrInstance = instNew;
                    sing.instanced = true;
                    return std::static_pointer_cast<void>(instNew);
                }
            };

            try
            {
                return std::static_pointer_cast<T>(ResolveErased_(
                    entryIt->second,
                    params,
                    tag,
                    GetGeneratedFn,
                    GetSingletonFn,
                    throwForKey
                ));
            }
            catch (const NotFound& e)
            {
                p2clog.note(std::format(L"Type found in service map but resolve failed [{}]", util::ToWide(typeid(T).name())))
                    .ex(NotFound{}).nested(e).commit();
                return {};
            }
        }
        static Services& Get_();
        static std::shared_ptr<void> ResolveErased_(
            Entry_& entry,
            const Params& params,
            const std::string& tag,
            ErasedGeneratorHelper_* GetGenerated,
            ErasedSingletonHelper_* GetSingleton,
            bool throwForKey);

    private:
        std::unordered_map<std::type_index, Entry_> typeServiceMap;
        std::shared_mutex readWriteMutex;
    };
}
