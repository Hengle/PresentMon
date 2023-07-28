#include "Framework.h"
#include <cassert>


namespace p2c::infra::opt::impl
{
	std::optional<CLI::App> AppBase_::app_;
	std::optional<AppBase_::CommandLine> AppBase_::line_;
	const char* AppBase_::group_ = nullptr;
	std::vector<const IEnumeratedOption_*> AppBase_::enumeratedOptionPtrs_;

	void AppBase_::InitInternal_(int argc, char** argv)
	{
		assert(!line_);
		line_.emplace(argc, argv);
		assert(!app_);
		app_.emplace();
	}

	CLI::App& AppBase_::GetApp_()
	{
		assert(app_);
		return *app_;
	}

	void AppBase_::Finalize_(const OptionStructBase_& options)
	{
		auto& app = GetApp_();
		assert(!app.parsed());

		app.description(options.GetDescription());
		app.name(options.GetName());
		app.allow_extras();
		try {
			app.parse(line_->argc, line_->argv);
		}
		catch (const CLI::ParseError& e) {
			app.exit(e);
			throw;
		};
	}

	void AppBase_::RegisterOption_(const IEnumeratedOption_* pOption)
	{
		enumeratedOptionPtrs_.push_back(pOption);
	}

	std::vector<ForwardingOption> AppBase_::GetForwardingOptions()
	{
		std::vector<ForwardingOption> opts;
		for (auto p : enumeratedOptionPtrs_) {
			auto opt = p->GetResults();
			if (opt.present) {
				opts.emplace_back(std::move(opt.name), std::move(opt.value));
			}
		}
		return opts;
	}


	Flag::Flag(std::string names, std::string description, bool forward)
		:
		forward_{ forward }
	{
		pOption_ = AppBase_::GetApp_().add_flag(std::move(names), value_, std::move(description));
		if (AppBase_::group_) {
			pOption_->group(AppBase_::group_);
		}
		AppBase_::RegisterOption_(this);
	}

	Flag::operator bool() const
	{
		return value_;
	}

	CLI::Option* Flag::opt() const { return pOption_; }

	Group::Group(const char* name)
	{
		AppBase_::group_ = name;
	}
}