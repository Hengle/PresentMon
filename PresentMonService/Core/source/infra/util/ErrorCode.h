// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <any>
#include <string>
#include <optional>
#include <type_traits>
#include <memory>

namespace p2c::infra::util
{
	class ErrorCode;

	class ErrorCodeTranslator
	{
	public:
		virtual ~ErrorCodeTranslator() = default;
		virtual std::wstring GetSymbol(const ErrorCode& code) const = 0;
		virtual std::wstring GetName(const ErrorCode& code) const = 0;
		virtual std::wstring GetDescription(const ErrorCode& code) const = 0;
		virtual std::wstring GetFormatted(const ErrorCode& code) const;
	};

	class ErrorCode
	{
	public:
		template<typename T>
		ErrorCode(T code_)
		{
			if constexpr (std::is_integral_v<T>)
			{
				intView = (long long)code_;
			}
			code = std::move(code_);
		}
		std::wstring GetSymbol() const;
		std::wstring GetName() const;
		std::wstring GetDescription() const;
		std::wstring GetFormatted() const;
		bool HasTranslator() const;
		std::shared_ptr<ErrorCodeTranslator> GetTranslator() const;
		std::optional<long long> GetIntegralView() const;
		operator const std::any&() const;

	private:
		std::any code;
		std::optional<long long> intView;
		mutable std::optional<std::shared_ptr<ErrorCodeTranslator>> pCachedTranslator;
	};
}