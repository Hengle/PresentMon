// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "DefaultFormatter.h"
#include "../EntryOutputBase.h"
#include <Core/source/infra/util/Exception.h>
#include <sstream>
#include <format>

namespace p2c::infra::log::fmt
{
	DefaultFormatter::DefaultFormatter(bool showTrace, bool showNested)
		:
		showTrace{ showTrace },
		showNested{ showNested }
	{}

	std::wstring DefaultFormatter::Format(const EntryOutputBase& entry) const
	{
		if (!entry.snapshot.empty())
		{
			return entry.snapshot;
		}

		std::wostringstream o;
		o << L"[@" << GetLevelName(entry.data.level) << L"] "
			<< L"{" << std::chrono::zoned_time{ std::chrono::current_zone(), entry.data.timestamp } << L"} ";
		if (entry.data.pid)
		{
			o << L"{PID:" << *entry.data.pid << L"} ";
		}
		if (entry.data.tid)
		{
			o << L"{TID:" << *entry.data.tid << L"} ";
		}
		if (const auto c = entry.data.code)
		{
			if (c->HasTranslator())
			{
				o << c->GetFormatted();
			}
			else if (auto intView = c->GetIntegralView())
			{
				o << std::format(L"CODE~<{}>", *intView);
			}
			else
			{
				o << "CODE=??";
			}
		}
		o << entry.data.note << " ";
		if (showTrace && entry.data.stackTrace)
		{
			o << "\n" << entry.data.stackTrace->Print();
		}
		o << std::format(L"\n>> at {}\n   {}({})", entry.data.functionName, entry.data.sourceFile, entry.data.sourceLine);

		if (auto pNested = entry.pNested; showNested && pNested)
		{
			if (auto p = dynamic_cast<const util::Exception*>(pNested))
			{
				o << std::format(L"\n%% Previous: [{}]\n{}",
					p->GetName(),
					p->FormatMessage()
				);
				if (showTrace && p->logData.stackTrace)
				{
					o << L"\n" << p->logData.stackTrace->Print();
				}
				o << std::format(L"(at [{}] : in {}, line {})", p->logData.functionName, p->logData.sourceFile, p->logData.sourceLine);
			}
			else
			{
				o << std::format(L"\n%% Previous: std::exception [{}]\n{}",
					util::ToWide(typeid(*pNested).name()),
					util::ToWide(pNested->what())
				);
			}
		}

		o << L"\n";

		return o.str();
	}
}