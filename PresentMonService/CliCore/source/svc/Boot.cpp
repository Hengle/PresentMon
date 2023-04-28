#include "Boot.h"
#include <Core/source/infra/svc/Services.h>
#include <Core/source/infra/util/FolderResolver.h>
#include <Core/source/infra/util/errtl/HResult.h>
#include <Core/source/infra/util/errtl/PMStatus.h>

namespace p2c::cli::svc
{
	void Boot()
	{
		using Services = infra::svc::Services;

		Services::Bind<infra::util::FolderResolver>(
			[] {return std::make_shared<infra::util::FolderResolver>(L"", false); }
		);

		// error code translators
		infra::util::errtl::HResult::RegisterService();
		infra::util::errtl::PMStatus::RegisterService();
	}
}