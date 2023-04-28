#pragma once
#include <unordered_set>
#include <string>
#include "Policy.h"

namespace p2c::infra::log
{
	class EntryOutputBase;

	class Blacklist
	{
	public:
		void Ingest(std::wstring path);
		void Insert(std::wstring file, int line);
		void Insert(std::wstring signature);
		Policy GetPolicy();
		bool IsEmpty() const;
	private:
		std::unordered_set<std::wstring> set;
	};
}