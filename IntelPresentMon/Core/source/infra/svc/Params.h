// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <any>
#include <variant>
#include <map>
#include <stdexcept>
#include <format>
#include <Core/source/infra/log/Logging.h>
#include "Exceptions.h"

namespace p2c::infra::svc
{
    class Params
    {
    private:
        using ContainerType = std::map<std::string, std::any>;
//        template<bool isConst>
//        class Proxy
//        {
//            template<class... Ts> struct Visitor : Ts... { using Ts::operator()...; };
//            using NodeType = typename std::conditional<isConst, const ContainerType, ContainerType>::type;
//            using LeafType = typename std::conditional<isConst, const std::any, std::any>::type;
//        public:
//            Proxy() = default;
//            Proxy(NodeType& node) : ptr{ &node } {}
//            Proxy(LeafType& leaf) : ptr{ &leaf } {}
//            Proxy operator[](const std::string& key) const
//            {
//                return std::visit(Visitor {
//                    [&key](NodeType* ptr) -> Proxy {
//                        try {
//                            std::any_cast<NodeTptr->at(std::string(key));
//                        } catch (const std::out_of_range& e) {
//                            p2clog.note(std::format(L"param failed struct access, missing key: {}", util::ToWide(key)));
//                            return {};
//                        }
//                    },
//                    [](LeafType* ptr) -> Proxy {
//                        p2clog.note(std::format(L"param failed struct access, was leaf type: {}",
//                            util::ToWide(ptr->type().name())
//                        ));
//                        return {};
//                    }
//                }, ptr);
//            }
//            template<typename T>
//            operator const T&() const
//            {
//                return std::visit(Visitor {
//                    [](NodeType* ptr) { p2clog.note(L"accessing value of non-leaf param node"); return T{}; },
//                    [](LeafType* ptr) {
//                        try { return std::any_cast<const T&>(*ptr); } catch (const std::bad_any_cast& e) {
//                            p2clog.note(std::format(L"param failed value access wrong type: {}; actual: {}\nPrevExc: {}",
//                                util::ToWide(typeid(T).name()),
//                                util::ToWide(ptr->type().name())),
//                                util::ToWide(e.what())
//                            );
//                        }
//                    },
//                }, ptr);
//            }
//        private:
//            std::variant<std::monostate, NodeType*, LeafType*> ptr;
//        };
    public:
        Params() = default;
        Params(std::initializer_list<ContainerType::value_type> elements);
        const std::any& GetAny(const std::string& key) const;
        std::any& GetAny(const std::string& key);
        template<typename T>
        T& Get(const std::string& key)
        {
            auto& any = GetAny(key);
            try {
                return std::any_cast<T&>(any);
            }
            catch (const std::bad_any_cast& e) {
                p2clog.note(std::format(L"Params::Get wrong type at @{}; wanted: {}; actual: {}",
                    util::ToWide(key),
                    util::ToWide(typeid(T).name()),
                    util::ToWide(any.type().name())
                )).ex(NotFound{}).nested(e).commit();
                throw;
            }
        }
        template<typename T>
        const T& Get(const std::string& key) const
        {
            return const_cast<Params*>(this)->Get<T>(key);
        }
        template<typename T>
        void Set(const std::string& key, T value)
        {
            container[key] = std::move(value);
        }
        Params& At(const std::string& key);
        const Params& At(const std::string& key) const;
        bool IsEmpty() const noexcept;
    private:
        ContainerType container;
    };
}
