// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "Driver.h"
#include <Core/source/infra/svc/Services.h>
#include "Formatter.h"
#include "fmt/DefaultFormatter.h"

namespace p2c::infra::log
{
	Driver::Driver(std::shared_ptr<Formatter> pFormatter)
	{
		if (pFormatter)
		{
			this->pFormatter = std::move(pFormatter);
		}
		else
		{
			this->pFormatter = std::make_unique<fmt::DefaultFormatter>();
		}
	}

	Driver::~Driver()
	{}

	void Driver::Accept(EntryOutputBase entry)
	{
		for (const auto& pol : policies)
		{
			if (!pol.Process(entry))
			{
				return;
			}
		}
		Commit(entry);
	}

	std::wstring Driver::FormatEntry(const EntryOutputBase& entry) const
	{
		return pFormatter->Format(entry);
	}
}