// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <Core/source/win/WinAPI.h>

#include <CppUnitTest.h>

#include <Core/source/infra/util/Exception.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

P2C_DEF_EX(CustomEx);

namespace InfrastructureTests
{
	using namespace std::string_literals;
	using namespace p2c;
	using namespace infra;
	using namespace util;

	TEST_CLASS(TestException)
	{
	public:
		TEST_METHOD(CatchCustomException)
		{
			try
			{
				CustomEx e;
				e.logData.sourceFile = L"C:\\FILE";
				e.logData.sourceLine = 420;
				e.logData.note = L"Hello from testland";
				throw e;
			}
			catch (const Exception& e) {
				Assert::AreEqual(e.GetName(), L"class CustomEx"s);
				return;
			}
		}
		TEST_METHOD(CloneExceptionWithNested)
		{
			CustomEx e;
			e.logData.sourceFile = L"C:\\FILE";
			e.logData.sourceLine = 420;
			e.logData.note = L"Hello from testland";
			e.logData.stackTrace.emplace();
			e.logData.code = (int)0x420;
				
			const util::Exception& ref = e;
			util::Exception e2;
			e2.logData.sourceFile = L"C:\\FILE2";
			e2.logData.sourceLine = 422;
			e2.logData.note = L"Hello from testland2";
			e2.logData.code = (int)69;
			e2.SetInner(ref);

			auto p = e2.Clone();

			Assert::AreEqual(p->GetName(), L"class p2c::infra::util::Exception"s);
			Assert::AreEqual(p->GetInner()->GetName(), L"class CustomEx"s);
		}
		TEST_METHOD(NestedWrappedStd)
		{
			CustomEx e;
			e.logData.sourceFile = L"C:\\FILE";
			e.logData.sourceLine = 420;
			e.logData.note = L"Hello from testland";
			e.logData.stackTrace.emplace();
			e.logData.code = (int)0x420;
			e.SetInner(std::runtime_error{ "hello" });

			const util::Exception& ref = e;
			util::Exception e2;
			e2.logData.sourceFile = L"C:\\FILE2";
			e2.logData.sourceLine = 422;
			e2.logData.note = L"Hello from testland2";
			e2.logData.code = (int)69;
			e2.SetInner(ref);

			auto p = e2.Clone();

			Assert::AreEqual(L"class p2c::infra::util::Exception"s, p->GetName());
			Assert::AreEqual(L"class CustomEx"s, p->GetInner()->GetName());
			Assert::AreEqual(L"class std::runtime_error"s, p->GetInner()->GetInner()->GetName());
			Assert::AreEqual(L"hello"s, p->GetInner()->GetInner()->FormatMessage());
		}
	};
}
