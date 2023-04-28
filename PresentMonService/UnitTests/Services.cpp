// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <Core/source/win/WinAPI.h>

#include <CppUnitTest.h>

#include <Core/source/infra/svc/Services.h>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace InfrastructureTests
{
	using namespace std::string_literals;
	using namespace p2c::infra::svc;

	struct IService
	{
		virtual std::string Speak() const = 0;
	};
	struct ServiceA : public IService
	{
		std::string Speak() const override
		{
			return "Hello From A";
		}
	};
	struct ServiceB : public IService
	{
		std::string Speak() const override
		{
			return "Hello From B";
		}
	};
	struct ServiceC : public IService
	{
		ServiceC(std::string s) : s{ s } {}
		std::string Speak() const override
		{
			return s;
		}
		std::string s;
	};

	TEST_CLASS(TestServices)
	{
	public:
		TEST_METHOD_CLEANUP(teardown_)
		{
			Services::Clear();
		}

		TEST_METHOD(Simple)
		{
			Services::Bind<IService>([] { return std::make_shared<ServiceA>(); });
			Assert::AreEqual(Services::Resolve<IService>()->Speak(), "Hello From A"s);
			Assert::IsTrue(&*Services::Resolve<IService>() != &*Services::Resolve<IService>());
		}
		TEST_METHOD(Singleton)
		{
			Services::Singleton<IService>([] { return std::make_shared<ServiceB>(); });
			Assert::AreEqual(Services::Resolve<IService>()->Speak(), "Hello From B"s);
			Assert::IsTrue(&*Services::Resolve<IService>() == &*Services::Resolve<IService>());
		}
		TEST_METHOD(Injected)
		{
			Services::InjectSingletonInstance<IService>(std::make_shared<ServiceA>());
			Assert::AreEqual(Services::Resolve<IService>()->Speak(), "Hello From A"s);
			Assert::IsTrue(&*Services::Resolve<IService>() == &*Services::Resolve<IService>());
		}
		TEST_METHOD(MultiTagging)
		{
			Services::InjectSingletonInstance<IService>(std::make_shared<ServiceA>());
			Services::Bind<IService>("tagA", [] { return std::make_shared<ServiceA>(); });
			Services::BindTypeTagged<IService, ServiceB>([] { return std::make_shared<ServiceB>(); });
			Assert::AreEqual(Services::Resolve<IService>()->Speak(), "Hello From A"s);
			Assert::AreEqual(Services::Resolve<IService>({}, "tagA")->Speak(), "Hello From A"s);
			Assert::AreEqual(Services::Resolve<IService>({}, typeid(ServiceB).name())->Speak(), "Hello From B"s);
			Assert::IsTrue(&*Services::Resolve<IService>({}, "tagA") != &*Services::Resolve<IService>({}, "tagA"));
			Assert::IsTrue(&*Services::Resolve<IService>({}, typeid(ServiceB).name()) != &*Services::Resolve<IService>({}, typeid(ServiceB).name()));
		}
		TEST_METHOD(ExistancePrimary)
		{
			Services::InjectSingletonInstance<IService>(std::make_shared<ServiceA>());
			Services::Bind<IService>("tagA", [] { return std::make_shared<ServiceA>(); });
			Assert::IsTrue((bool)Services::ResolveOrNull<IService>());
			Assert::IsFalse((bool)Services::ResolveOrNull<int>());
		}
		TEST_METHOD(ParameterizedPrimary)
		{
			Services::Bind<IService>([](const Params& p) { return std::make_shared<ServiceC>(p.Get<std::string>("str")); });
			Assert::AreEqual(Services::Resolve<IService>({ {"str", "Hi C"s} })->Speak(), "Hi C"s);
		}
		TEST_METHOD(ParameterizedMultitagged)
		{
			Services::Bind<IService>("tagC", [](const Params& p) { return std::make_shared<ServiceC>(p.Get<std::string>("str")); });
			Assert::AreEqual(Services::Resolve<IService>({ {"str", "Hi C"s} }, "tagC")->Speak(), "Hi C"s);
		}
	};
}
