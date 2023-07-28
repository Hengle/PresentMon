// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <Core/source/win/WinAPI.h>

#include <CppUnitTest.h>

#include <Core/source/infra/svc/Params.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace InfrastructureTests
{
	using namespace std::string_literals;

	TEST_CLASS(TestServiceParams)
	{
		using Params = p2c::infra::svc::Params;
	public:		
		TEST_METHOD(SimpleSetGet)
		{
			Params p;
			p.Set("int-val", 16);
			Assert::AreEqual(p.Get<int>("int-val"), 16);
		}
		TEST_METHOD(NestedSetGet)
		{
			Params outer;
			outer.Set("inner", Params{});
			outer.Get<Params>("inner").Set("int-val", 420);
			Assert::AreEqual(outer.Get<Params>("inner").Get<int>("int-val"), 420);
		}
		TEST_METHOD(NestedAtSetGet)
		{
			Params outer;
			outer.Set("inner", Params{});
			outer.At("inner").Set("int-val", 420);
			Assert::AreEqual(outer.At("inner").Get<int>("int-val"), 420);
		}
		TEST_METHOD(InitFlat)
		{
			const Params p{{"alpha", 12}, {"beta", "b"s}};
			Assert::AreEqual(p.Get<int>("alpha"), 12);
			Assert::AreEqual(p.Get<std::string>("beta"), "b"s);
		}
		TEST_METHOD(InitNested)
		{
			const Params p{
				{"alpha", 12},
				{"beta", "b"s},
				{"inner", Params{
					{"gamma", L"g"s}
				}}
			};
			Assert::AreEqual(p.Get<int>("alpha"), 12);
			Assert::AreEqual(p.Get<std::string>("beta"), "b"s);
			Assert::AreEqual(p.At("inner").Get<std::wstring>("gamma"), L"g"s);
		}
	};
}
