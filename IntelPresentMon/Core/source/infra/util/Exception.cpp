// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Exception.h"
#include <typeinfo>
#include <format>
#include <Core/source/infra/util/Util.h>

namespace p2c::infra::util
{
	// base Exception class

	Exception::Exception(const Exception& src)
		:
		logData{ src.logData },
		pInnerException{ src.pInnerException ? src.pInnerException->Clone() : std::unique_ptr<Exception>{} }
	{}

	Exception& Exception::operator=(const Exception& rhs)
	{
		if (&rhs != this) {
			logData = rhs.logData;
			pInnerException = rhs.pInnerException ? rhs.pInnerException->Clone() : std::unique_ptr<Exception>{};
		}
		return *this;
	}

	Exception::Exception(std::wstring message)
		:
		logData{ .note = std::move(message) }
	{}

	std::wstring Exception::GetName() const
	{
		return util::ToWide(typeid(const_cast<Exception&>(*this)).name());
	}

	const Exception* Exception::GetInner() const
	{
		return pInnerException.get();
	}

	void Exception::SetInner(const std::exception& e)
	{
		if (auto p = dynamic_cast<const Exception*>(&e))
		{
			pInnerException = p->Clone();
		}
		else
		{
			pInnerException = std::make_unique<WrappedStdException>(e);
		}
	}

	std::wstring Exception::FormatMessage() const
	{
		return std::format(L"[{}] {} {} {}{}",
			log::GetLevelName(logData.level),
			logData.code ? logData.code->GetFormatted() : std::wstring{},
#ifndef NDEBUG
			std::format(L"{}:{}", logData.sourceFile, logData.sourceLine),
#else
			std::wstring{},
#endif
			logData.note,
			pInnerException ? std::format(L"\n\nInner: {}\n{}", pInnerException->GetName(), pInnerException->FormatMessage()) : std::wstring{}
		);
	}

	const char* Exception::what() const
	{
		auto message = util::ToNarrow(FormatMessage());
		return (whatBuffer = std::move(message)).c_str();
	}

	std::unique_ptr<Exception> Exception::Clone() const
	{
		return std::make_unique<Exception>(*this);
	}


	// wrapped STD exception

	WrappedStdException::WrappedStdException(const std::exception& e)
		:
		name{ util::ToWide(typeid(e).name()) }
	{
		logData.note = util::ToWide(e.what());
	}

	std::wstring WrappedStdException::GetName() const
	{
		return name;
	}

	std::wstring WrappedStdException::FormatMessage() const
	{
		return logData.note;
	}
}