#pragma once
#include <unordered_map>
#include <string>
#include <fstream>
#include "Policy.h"
#include <Core/source/infra/util/Hash.h>

namespace p2c::infra::log
{
	class EntryOutputBase;

	class Blacklist
	{
	public:
		enum class Type
		{
			Block,
			Trace,
		};
		void Ingest(const std::wstring& path);
		void Ingest(std::wistream& stream);
		void Ingest(std::wistream&& stream);
		void Insert(std::wstring file, int line, Type type);
		Policy GetPolicy();
		bool IsEmpty() const;
	private:
		void ParseAndInsert(const std::wstring& listLine);
		std::unordered_map<std::pair<int, std::wstring>, Type> map;
	};
}