// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <vector>
#include <memory>
#include "Policy.h"

namespace p2c::infra::log
{
	class EntryOutputBase;
	class Formatter;

	class Driver
	{
	public:
		Driver(std::shared_ptr<Formatter> pFormatter = {});
		virtual ~Driver();
		virtual void Commit(const EntryOutputBase&) = 0;
		void Accept(EntryOutputBase entry);
		std::wstring FormatEntry(const EntryOutputBase&) const;
	private:
		std::vector<Policy> policies;
		std::shared_ptr<Formatter> pFormatter;
	};
}