// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include <Core/source/win/WinAPI.h>

#include <CppUnitTest.h>

#include <Core/source/infra/log/Logging.h>
#include <Core/source/infra/svc/Services.h>
#include <Core/source/infra/util/errtl/HResult.h>
#include <Core/source/infra/util/Exception.h>
#include <fstream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

P2C_DEF_EX(CustomEx);
P2C_DEF_EX(CustomEx2);

namespace InfrastructureTests
{
	using namespace std::string_literals;
	using namespace p2c;
	using namespace p2c::infra;

	TEST_CLASS(TestLogging)
	{
	public:
		TEST_METHOD_CLEANUP(teardown_)
		{
			using svc::Services;
			Services::Clear();
		}

		TEST_METHOD(HresultTranslationShortcut)
		{
			using svc::Services;
			Services::Singleton<util::ErrorCodeTranslator>(typeid(util::errtl::HRWrap).name(), [] { return std::make_shared<util::errtl::HResult>(); });
			p2clog.hr(DRAGDROP_S_CANCEL).warn().flush().commit();
			Assert::IsTrue(true);
		}
		TEST_METHOD(HresultTranslation)
		{
			using svc::Services;
			Services::Singleton<util::ErrorCodeTranslator>(typeid(util::errtl::HRWrap).name(), [] { return std::make_shared<util::errtl::HResult>(); });
			p2clog.code(util::errtl::HRWrap{ S_OK }).info().flush().commit();
			Assert::IsTrue(true);
		}
		TEST_METHOD(HresultTranslationMissingNonInt)
		{
			p2clog.code(3.14f).warn().commit();
			Assert::IsTrue(true);
		}
		TEST_METHOD(HresultTranslationMissing)
		{
			p2clog.code(420).warn().commit();
			Assert::IsTrue(true);
		}
		TEST_METHOD(ThrowCustomException)
		{
			try
			{
				p2clog.code(420).warn().ex(CustomEx{}).commit();
			}
			catch (const CustomEx& e)
			{
				std::string w = e.what();
				Assert::AreEqual(e.GetName(), L"class CustomEx"s);
			}
			catch (...)
			{
				Assert::Fail();
			}
		}
		TEST_METHOD(HandleNestedStdException)
		{
			try
			{
				try
				{
					throw std::runtime_error{ "juice me" };
				}
				catch (const std::exception& e)
				{
					p2clog.code(420).ex(CustomEx{}).nested(e).commit();
				}
				catch (...)
				{
					Assert::Fail();
				}
			}
			catch (const util::Exception& e)
			{
				Assert::AreEqual(L"class CustomEx"s, e.GetName());
				Assert::AreEqual(L"class std::runtime_error"s, e.GetInner()->GetName());
				Assert::AreEqual(L"juice me"s, e.GetInner()->FormatMessage());
				Assert::AreEqual(L"juice me"s, e.GetInner()->logData.note);
			}
			catch (...)
			{
				Assert::Fail();
			}
		}
		TEST_METHOD(HandleNestedCustomException)
		{
			svc::Services::Singleton<util::ErrorCodeTranslator>(
				typeid(util::errtl::HRWrap).name(),
				[] { return std::make_shared<util::errtl::HResult>(); }
			);

			try
			{
				try
				{
					p2clog.hr(DRAGDROP_S_CANCEL).note(L"inner note").ex(CustomEx2{}).commit();
				}
				catch (const std::exception& e)
				{
					p2clog.code(420).ex(CustomEx{}).nested(e).commit();
				}
				catch (...)
				{
					Assert::Fail();
				}
			}
			catch (const util::Exception& e)
			{
				Assert::AreEqual(L"class CustomEx"s, e.GetName());
				Assert::AreEqual(420ll, *e.logData.code->GetIntegralView());
				Assert::AreEqual(L"class CustomEx2"s, e.GetInner()->GetName());
				Assert::AreEqual(L"DRAGDROP_S_CANCEL"s, e.GetInner()->logData.code->GetName());
				Assert::AreEqual(DRAGDROP_S_CANCEL, std::any_cast<util::errtl::HRWrap>(*e.GetInner()->logData.code).val);
				Assert::AreEqual(L"inner note"s, e.GetInner()->logData.note);
			}
			catch (...)
			{
				Assert::Fail();
			}
		}
	};
}
