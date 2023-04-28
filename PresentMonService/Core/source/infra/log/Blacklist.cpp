#include "Blacklist.h"
#include <fstream>

namespace p2c::infra::log
{
	void Blacklist::Ingest(std::wstring path)
	{
		std::wifstream file{ path };
		std::wstring line;
#pragma warning (push)
#pragma warning (disable : 26800)
		while (std::getline(file, line)) {
			Insert(std::move(line));
		}
#pragma warning (pop)
	}
	void Blacklist::Insert(std::wstring file, int line)
	{
		file.append(std::to_wstring(line));
		Insert(std::move(file));
	}
	void Blacklist::Insert(std::wstring signature)
	{
		set.insert(std::move(signature));
	}
	Policy Blacklist::GetPolicy()
	{
		return { [this](EntryOutputBase& entry) -> bool {
			auto sig = entry.data.sourceFile;
			sig.append(std::to_wstring(entry.data.sourceLine));
			return !set.contains(sig);
		} };
	}
	bool Blacklist::IsEmpty() const
	{
		return set.empty();
	}
}