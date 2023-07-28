#include "Blacklist.h"
#include <fstream>
#include <regex>
#include <utility>
#include <optional>
#include <stdexcept>

namespace p2c::infra::log
{
	void Blacklist::Ingest(const std::wstring& path)
	{
		Ingest(std::wifstream{ path });
	}
	void Blacklist::Ingest(std::wistream&& stream)
	{
		Ingest(stream);
	}
	void Blacklist::Ingest(std::wistream& stream)
	{
		std::wstring line;
		while (std::getline(stream, line)) {
			ParseAndInsert(line);
		}
	}
	void Blacklist::Insert(std::wstring path, int line, Type type)
	{
		map.emplace(std::piecewise_construct,
			std::forward_as_tuple(line, std::move(path)),
			std::forward_as_tuple(type)
		);
	}
	Policy Blacklist::GetPolicy()
	{
		return { [this](EntryOutputBase& entry) -> bool {
			const auto sig = std::make_pair(entry.data.sourceLine, entry.data.sourceFile);
			if (const auto i = map.find(sig); i != map.end()) {
				if (i->second == Type::Block) {
					return false;
				}
				else if (i->second == Type::Trace) {
					entry.tracing = true;
				}
			}
			return true;
		} };
	}
	void Blacklist::ParseAndInsert(const std::wstring& listLine)
	{
		auto type = Type::Block;
		if (!listLine.empty() && listLine[0] == L'*') {
			type = Type::Trace;
		}
		std::wsmatch match;
		if (std::regex_search(listLine, match, std::wregex{ LR"(\*?\s*([^()]+)\((\d+)\))" })) {
			auto path = match[1].str();
			auto line_number = std::stoi(match[2].str());
			Insert(std::move(path), line_number, type);
		}
	}
	bool Blacklist::IsEmpty() const
	{
		return map.empty();
	}
}