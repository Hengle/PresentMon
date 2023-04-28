// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Entry.h"
#include "Logging.h"
#include <Core/source/win/WinAPI.h>
#include <Core/source/infra/util/errtl/HResult.h>
#include <Core/source/infra/util/Exception.h>
#include <Core/source/infra/svc/Services.h>


namespace p2c::infra::log
{
	Entry::Entry(std::wstring sourceFile, int sourceLine, std::wstring functionName)
		:
		EntryOutputBase{ std::move(sourceFile), sourceLine, std::move(functionName) }
	{
		static_cast<void>(level(Level::Error));
	}

	Entry::~Entry()
	{
		if (!committed)
		{
			p2clog.warn(L"Log entry abandoned without committing").nox().commit();
			nox().commit();
		}
	}

	Entry& Entry::hr(std::optional<HRESULT> code)
	{
		if (code)
		{
			data.code = util::errtl::HRWrap{ *code };
		}
		else
		{
			data.code = util::errtl::HRWrap{ (long)GetLastError() };
		}
		return *this;
	}

	Entry& Entry::pid()
	{
		data.pid = GetProcessId(GetCurrentProcess());
		return *this;
	}

	Entry& Entry::tid()
	{
		data.tid = GetCurrentThreadId();
		return *this;
	}

	Entry& Entry::level(Level level)
	{
		data.level = level;
		switch (data.level)
		{
		case Level::Error:
			throwing = true;
			tracing = true;
			flushing = true;
			logging = true;
			break;
		case Level::Warning:
			throwing = false;
			tracing = false;
			flushing = false;
			logging = true;
			break;
		case Level::Info:
		case Level::Debug:
		case Level::Verbose:
			throwing = false;
			tracing = false;
			flushing = false;
			logging = true;
			break;
		}
		return *this;
	}

	Entry& Entry::info(std::optional<std::wstring> note)
	{
		if (note)
		{
			static_cast<void>(this->note(std::move(*note)));
		}
		return level(Level::Info);
	}

	Entry& Entry::warn(std::optional<std::wstring> note)
	{
		if (note)
		{
			static_cast<void>(this->note(std::move(*note)));
		}
		return level(Level::Warning);
	}

	Entry& Entry::verbose(std::optional<std::wstring> note)
	{
		if (note)
		{
			static_cast<void>(this->note(std::move(*note)));
		}
		return level(Level::Verbose);
	}

	Entry& Entry::note(std::wstring note)
	{
		data.note = std::move(note);
		return *this;
	}

	Entry& Entry::trace()
	{
		tracing = true;
		return *this;
	}

	Entry& Entry::notrace()
	{
		tracing = false;
		return *this;
	}

	Entry& Entry::nox()
	{
		throwing = false;
		return *this;
	}

	Entry& Entry::flush()
	{
		flushing = true;
		return *this;
	}

	Entry& Entry::nested(const std::exception& e)
	{
		throwing = true;
		pNested = &e;
		return *this;
	}

	Entry& Entry::log()
	{
		logging = true;
		return *this;
	}

	void Entry::commit()
	{
		if (committed)
		{
			p2clog.warn(L"Trying to commit log entry twice").nox().commit();
			return;
		}
		committed = true;

		if (pChannel)
		{
			pChannel->Accept(*this);
		}
		else
		{
			GetDefaultChannel()->Accept(*this);
		}
	}

	Entry& Entry::chan(const std::string& tag)
	{
		return chan(svc::Services::Resolve<Channel>({}, tag));
	}

	Entry& Entry::chan(std::shared_ptr<Channel> pChannel)
	{
		this->pChannel = std::move(pChannel);
		return *this;
	}
}