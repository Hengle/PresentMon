// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <exception>
#include <string>
#include <optional>
#include <memory>
#include <any>
#include "Util.h"
#include <Core/source/infra/log/Logging.h>


namespace p2c::infra::util
{
	class Exception : public std::exception
	{
	public:
		Exception() = default;
		Exception(std::wstring message);
		Exception(const Exception& src);
		Exception& operator=(const Exception&);
		Exception(Exception&&) = default;
        ~Exception() = default;
		const Exception* GetInner() const;
		void SetInner(const std::exception& e);
		virtual std::wstring GetName() const;
		virtual std::wstring FormatMessage() const;
		virtual std::unique_ptr<Exception> Clone() const;
		const char* what() const override;
		// data
		log::LogData logData;
	private:
		std::unique_ptr<Exception> pInnerException;
		mutable std::string whatBuffer;
	};

	template<class Derived>
	class ClonableException : public Exception
	{
	public:
		using Exception::Exception;
		std::unique_ptr<Exception> Clone() const override
		{
			return std::make_unique<Derived>(*static_cast<const Derived*>(this));
		}
	};

	class WrappedStdException final : public ClonableException<WrappedStdException>
	{
	public:
		WrappedStdException(const std::exception& e);
		std::wstring GetName() const override;
		std::wstring FormatMessage() const override;
	private:
		std::wstring name;
	};
}

// macro to define custom exception types inheriting from infrastructure common base
#define P2C_DEF_EX(ExceptionType) class ExceptionType : public p2c::infra::util::ClonableException<ExceptionType> {public: using ClonableException::ClonableException;}