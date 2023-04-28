#pragma once
#include "Framework.h"
#include <string>
#include <vector>

namespace p2c::cli::opt
{
	namespace impl
	{
		struct OptionsStruct : public OptionStructBase_
		{
			// add options and switches here to augment the CLI
			Option<std::vector<int>> pid{ "--pid", "ID(s) of process(es) to capture", [this](CLI::Option* p) {
				p->check(CLI::PositiveNumber); }};
			Option<std::vector<std::string>> name{ "--name", "Name(s) of process(es) to capture" };
			Flag multiCsv{ "-m,--multi-csv", "Write frame data from each process to a separate CSV file" };
			Flag ignoreCase{ "-i,--ignore-case", "Ignore case when matching a process name" };
			Option<double> startAfter{ "--start-after", "Time in seconds from launch to start of capture" };
			Option<double> captureFor{ "--capture-for", "Time in seconds from start of capture to exit" };
			Option<std::vector<std::string>> group{ "--enable-group", "CSV column group(s) to enable" };
			Flag disableCore{ "-c,--disable-core", "Disable core (default) CSV columns" };
			Flag captureAll{ "-A,--capture-all", "Capture all processes presenting on system", [this](CLI::Option* p) {
				p->excludes(pid.opt(), name.opt()); } };
			Option<std::vector<std::string>> excludeName{ "--exclude-name", "Name(s) of process(es) to exclude", [this](CLI::Option* p) {
				p->excludes(pid.opt(), name.opt()); } };
			Flag excludeDropped{ "-d,--exclude-dropped", "Exclude dropped frames from capture" };
			Flag outputStdout{ "-s,--output-stdout", "Write frame data to stdout instead of status messages", [this](CLI::Option* p) {
				p->excludes(multiCsv.opt()); } };
			Flag noCsv{ "-v,--no-csv", "Disable CSV file output", [this](CLI::Option* p) {
				p->needs(outputStdout.opt()); } };
			Option<std::string> outputFile{ "-o,--output-file", "Path, root name, and extension used for CSV file(s)" };
			Flag listAdapters{ "-a,--list-adapters", "List adaptors available for selection as telemetry source" };
			Option<int> adapter{ "--adapter", "Index of adapter to use as telemetry source", [this](CLI::Option* p) {
				p->check(CLI::NonNegativeNumber); } };
			Option<std::string> etlFile{ "--etl-file", "Source frame data from an externally-captured ETL file instead of live ETW events" };
			Option<int> telemetryPeriod{ "--telemetry-period", "Period that the service uses to poll hardware telemetry", [this](CLI::Option* p) {
				p->check(CLI::PositiveNumber); } };
			Option<int> pollPeriod{ "--poll-period", "Period that this client uses to poll the service for frame data", [this](CLI::Option* p) {
				p->check(CLI::PositiveNumber); } };

		protected:
			// edit application name and description here
			std::string GetName() const override
			{ return "PresentMonCli"; }
			std::string GetDescription() const override
			{ return "Command line interface for capturing frame presentation data using the PresentMon service."; }
		};
	}

	inline void init(int argc, char** argv) { impl::App<impl::OptionsStruct>::Init(argc, argv); }
	inline auto& get() { return impl::App<impl::OptionsStruct>::GetOptions(); }
}