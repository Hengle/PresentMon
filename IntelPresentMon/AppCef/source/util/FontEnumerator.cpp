// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "FontEnumerator.h"
#include <Core/source/win/WinAPI.h>
#include <Core/source/gfx/base/ComPtr.h>
#include <Core/source/infra/log/Logging.h>
#include <dwrite.h>


namespace p2c::client::util
{
    using gfx::ComPtr;

	FontEnumerator::FontEnumerator()
	{
        ComPtr<IDWriteFactory> pWriteFactory;
        p2chrlog << DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            (IUnknown**)&pWriteFactory
        );
        ComPtr<IDWriteFontCollection> pFonts;
        p2chrlog << pWriteFactory->GetSystemFontCollection(&pFonts);
        wchar_t locale[LOCALE_NAME_MAX_LENGTH];
        if (!GetUserDefaultLocaleName(locale, (int)std::size(locale)))
        {
            p2clog.hr().commit();
        }
        unsigned const count = pFonts->GetFontFamilyCount();
        for (unsigned familyIndex = 0; familyIndex != count; ++familyIndex)
        {
            ComPtr<IDWriteFontFamily> pFamily;
            p2chrlog << pFonts->GetFontFamily(familyIndex, &pFamily);
            ComPtr<IDWriteLocalizedStrings> pNames;
            p2chrlog << pFamily->GetFamilyNames(&pNames);

            unsigned nameIndex;
            BOOL exists;
            p2chrlog << pNames->FindLocaleName(locale, &nameIndex, &exists);
            if (exists)
            {
                wchar_t name[64];
                p2chrlog << pNames->GetString(nameIndex, name, (unsigned)std::size(name));
                names.push_back(name);
            }
            else if (pNames->GetCount() > 0)
            {
                wchar_t name[64];
                p2chrlog << pNames->GetString(0, name, (unsigned)std::size(name));
                names.push_back(name);
            }
        }
	}

    const std::vector<std::wstring>& FontEnumerator::GetNames() const
    {
        return names;
    }
}
