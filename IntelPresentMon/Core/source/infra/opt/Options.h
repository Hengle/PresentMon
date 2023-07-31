#pragma once
#include "Framework.h"
#include <string>
#include <vector>

namespace p2c::infra::opt
{
	namespace impl
	{
		struct OptionsStruct : public OptionStructBase_
		{
			// add options and switches here to augment the CLI
			Flag verbose{ "--p2c-verbose", "Enable verbose logging in release" };
			Flag filesWorking{ "--p2c-files-working", "Use the working directory for file storage" };
			Flag logBlack{ "--p2c-log-black", "Use blacklist to exclude source lines from logging" };
			Flag noNetFail{ "--p2c-no-net-fail", "Disable error modal for bad url accesses" };
			Flag allowTearing{ "--p2c-allow-tearing", "Allow tearing presents (optional, might affect VRR)" };
			Option<std::string> url{ "--p2c-url", "URL to load instead of app files" };
			Option<std::string> cefType{ "--type", "Type of the current chromium process", false };

		protected:
			// edit application name and description here
			std::string GetName() const override
			{ return "PresentMon"; }
			std::string GetDescription() const override
			{ return "PresentMon performance overlay and trace capture application"; }
		};
	}

	inline void init() { impl::App<impl::OptionsStruct>::Init(__argc, __argv); }
	inline auto& get() { return impl::App<impl::OptionsStruct>::GetOptions(); }
	inline auto get_forwarding() { return impl::App<impl::OptionsStruct>::GetForwardingOptions(); }
}