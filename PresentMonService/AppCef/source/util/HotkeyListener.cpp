// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "HotkeyListener.h"
#include <Core/source/win/WinAPI.h>
#include <Core/source/infra/log/Logging.h>
#include <include/cef_task.h>
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"


// TODO: general logging in this codebase (winapi calls like PostThreadMessage etc.)

namespace p2c::client::util
{
	namespace HotkeyMsg
	{
		static constexpr uint32_t Bind = WM_USER + 100;
		static constexpr uint32_t Clear = WM_USER + 101;
	}

	// used to pass info for hotkey bind operation via pointer in message queue
	struct BindPacket_
	{
		Action action;
		win::Key key;
		win::ModSet mods;
		std::function<void(bool)> resultCallback;
	};

	Hotkeys::Hotkeys()
		:
		thread{ &Hotkeys::Kernel_, this }
	{
		sem.acquire();
	}

	Hotkeys::~Hotkeys()
	{
		PostThreadMessageA(tid, WM_QUIT, 0, 0);
	}

	void Hotkeys::SetHandler(std::function<void(Action)> handler)
	{
		std::lock_guard lk{ mtx };
		Handle_ = std::move(handler);
	}

	void Hotkeys::Kernel_()
	{
		tid = GetCurrentThreadId();
		sem.release();

		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			if (msg.message == HotkeyMsg::Bind)
			{
				std::unique_ptr<BindPacket_> pPacket{ reinterpret_cast<BindPacket_*>(msg.lParam) };
				try
				{
					BindAction_(pPacket->action, pPacket->key, pPacket->mods);
					pPacket->resultCallback(true);
				}
				catch (...)
				{
					pPacket->resultCallback(false);
				}
			}
			else if (msg.message == HotkeyMsg::Clear)
			{
				std::unique_ptr<std::function<void(bool)>> pCallback{ reinterpret_cast<std::function<void(bool)>*>(msg.lParam) };
				try
				{
					ClearAction_(Action(msg.wParam));
					(*pCallback)(true);
				}
				catch (...)
				{
					(*pCallback)(false);
				}
			}
			else if (msg.message == WM_HOTKEY)
			{
				if (msg.wParam < int32_t(Action::Count_))
				{
					CefPostTask(TID_RENDERER, base::BindOnce(&Hotkeys::DispatchHotkey_, base::Unretained(this), (Action)msg.wParam));
				}
			}
		}
		// clear all hotkeys when exiting thread (have to unreg on this kernel thread, so dont let dtor (called on different thread) clear)
		registeredHotkeys.clear();
	}

	void Hotkeys::BindAction(Action action, win::Key key, win::ModSet mods, std::function<void(bool)> resultCallback)
	{
		PostThreadMessageA(tid, HotkeyMsg::Bind, 0, reinterpret_cast<LPARAM>(new BindPacket_{
			action,
			key,
			mods,
			std::move(resultCallback)
		}));
	}

	void Hotkeys::ClearAction(Action action, std::function<void(bool)> resultCallback)
	{
		PostThreadMessageA(tid, HotkeyMsg::Clear, WPARAM(action),
			reinterpret_cast<LPARAM>(new decltype(resultCallback){ std::move(resultCallback)})
		);
	}

	void Hotkeys::BindAction_(Action action, win::Key key, win::ModSet mods)
	{
		if (const auto i = registeredHotkeys.find(action); i != registeredHotkeys.cend())
		{
			if (!i->second.CombinationMatches(key, mods))
			{
				i->second.ChangeCombination(key, mods);
			}
		}
		else
		{
			registeredHotkeys.emplace(std::piecewise_construct,
				std::forward_as_tuple(action),
				std::forward_as_tuple(action, key, mods)
			);
		}
	}

	void Hotkeys::ClearAction_(Action action)
	{
		if (const auto i = registeredHotkeys.find(action); i != registeredHotkeys.cend())
		{
			registeredHotkeys.erase(i);
		}
		else
		{
			p2clog.note(L"action hotkey not found").notrace().commit();
		}
	}

	void Hotkeys::DispatchHotkey_(Action action) const
	{
		std::lock_guard lk{ mtx };
		if (Handle_)
		{
			Handle_(action);
		}
	}

	Hotkeys::Hotkey::Hotkey(Action action, win::Key key, win::ModSet mods)
		:
		action{ action },
		key{ key },
		mods{ mods }
	{
		if (!RegisterHotKey(nullptr, (int)action, mods.GetCode(), key.GetPlatformCode()))
		{
			p2clog.warn(L"failed registering hotkey").hr().commit();
			throw std::runtime_error{""};
		}
	}

	Hotkeys::Hotkey::~Hotkey()
	{
		try { Unregister_(); }
		catch (...) {}
	}

	bool Hotkeys::Hotkey::CombinationMatches(win::Key key_, win::ModSet mods_) const
	{
		return key == key_ && mods == mods_;
	}

	void Hotkeys::Hotkey::ChangeCombination(win::Key key_, win::ModSet mods_)
	{
		Unregister_();
		try
		{
			Register_(action, key_, mods_);
			key = key_;
			mods = mods_;
		}
		catch (...)
		{
			Register_(action, key, mods);
			p2clog.warn(L"failed changing hotkey").hr().commit();
			throw std::runtime_error{ "" };
		}
	}

	void Hotkeys::Hotkey::Unregister_()
	{
		if (!UnregisterHotKey(nullptr, (int)action))
		{
			p2clog.warn(L"failure to unregister hotkey").hr().nox().commit();
		}
	}

	void Hotkeys::Hotkey::Register_(Action action, win::Key key, win::ModSet mods)
	{
		if (!RegisterHotKey(nullptr, (int)action, mods.GetCode(), key.GetPlatformCode()))
		{
			p2clog.warn(L"failed registering hotkey").hr().commit();
			throw std::runtime_error{ "" };
		}
	}
}
