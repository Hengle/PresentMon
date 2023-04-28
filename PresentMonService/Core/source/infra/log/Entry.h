// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <optional>
#include <exception>
#include "EntryOutputBase.h"
#include "Level.h"
#include "Channel.h"
#include "DefaultChannel.h"

typedef long HRESULT;

namespace p2c::infra::log
{
	class [[nodiscard]] Entry : private EntryOutputBase
	{
		friend class Channel;
	public:
		Entry(std::wstring sourceFile, int sourceLine, std::wstring functionName);
		~Entry();
		template<typename C>
		[[nodiscard]] Entry& code(C code)
		{
			data.code = code;
			return *this;
		}
		[[nodiscard]] Entry& level(Level level);
		[[nodiscard]] Entry& hr(std::optional<HRESULT> code = {});
		[[nodiscard]] Entry& info(std::optional<std::wstring> note = {});
		[[nodiscard]] Entry& warn(std::optional<std::wstring> note = {});
		[[nodiscard]] Entry& verbose(std::optional<std::wstring> note = {});
		[[nodiscard]] Entry& note(std::wstring note);
		[[nodiscard]] Entry& pid();
		[[nodiscard]] Entry& tid();
		[[nodiscard]] Entry& trace();
		[[nodiscard]] Entry& log();
		[[nodiscard]] Entry& notrace();
		[[nodiscard]] Entry& nox();
		[[nodiscard]] Entry& flush();
		[[nodiscard]] Entry& nested(const std::exception& e);
		[[nodiscard]] Entry& chan(const std::string& tag);
		[[nodiscard]] Entry& chan(std::shared_ptr<Channel> pChannel);
		template<class E>
		[[nodiscard]] Entry& ex(E exception)
		{
			throwing = true;
			exceptinator = [e = std::move(exception)](LogData data, const std::exception* pNested) mutable {
				e.logData = std::move(data);
				if (pNested)
				{
					e.SetInner(*pNested);
				}
				throw e;
			};
			return *this;
		}
		void commit();
	private:
		// data
		bool committed = false;
		std::shared_ptr<Channel> pChannel;
	};
}