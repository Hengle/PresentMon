// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#include "HotkeyListener.h"
#include <Core/source/win/WinAPI.h>
#include <Core/source/infra/log/Logging.h>
#include <Core/source/infra/log/v/Hotkey.h>
#include <ranges>
#include <include/cef_task.h> 
#include "include/base/cef_callback.h" 
#include "include/wrapper/cef_closure_task.h" 

namespace rn = std::ranges;
namespace vi = rn::views;
using p2c::infra::log::v::hotkey;

#define vlog p2cvlog(hotkey)

// TODO: general logging in this codebase (winapi calls like PostThreadMessage etc.)

namespace p2c::client::util
{
	Hotkeys::Hotkeys()
		:
		thread_{ &Hotkeys::Kernel_, this }
	{
		startupSemaphore_.acquire();
		vlog.note(L"Hotkey process ctor complete").commit();
	}
	Hotkeys::~Hotkeys()
	{
		vlog.note(L"Destroying hotkey processor").commit();
		PostThreadMessageA(threadId_, WM_QUIT, 0, 0);
	}
	void Hotkeys::Kernel_()
	{
		vlog.note(L"Hotkey processor kernel start").tid().commit();

		// capture thread id
		threadId_ = GetCurrentThreadId();

		// create message window class
		const WNDCLASSEXA wx = {
			.cbSize = sizeof(WNDCLASSEX),
			.lpfnWndProc = DefWindowProc,
			.hInstance = GetModuleHandle(nullptr),
			.lpszClassName = "$PresentmonMessageWindowClass$",
		};
		const auto atom = RegisterClassEx(&wx);
		if (!atom) {
			p2clog.nox().commit();
			return;
		}

		vlog.note(std::format(L"Hotkey processor wndclass registered: {:X}", atom)).commit();

		// create message window
		messageWindowHandle_ = CreateWindowExA(
			0, MAKEINTATOM(atom), "$PresentmonMessageWindow$",
			0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr
		);
		if (!messageWindowHandle_) {
			p2clog.nox().commit();
			return;
		}

		vlog.note(std::format(L"Hotkey processor wnd created: {:X}",
			reinterpret_cast<uintptr_t>(messageWindowHandle_))).commit();

		// register to receive raw keyboard input
		const RAWINPUTDEVICE rid{
			.usUsagePage = 0x01,
			.usUsage = 0x06,
			.dwFlags = RIDEV_INPUTSINK,
			.hwndTarget = static_cast<HWND>(messageWindowHandle_),
		};
		if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
			p2clog.nox().commit();
			return;
		}

		vlog.note(L"Raw input registered").commit();

		// signal that constuction is complete
		startupSemaphore_.release();

		MSG msg;
		while (GetMessageA(&msg, nullptr, 0, 0)) {
			if (msg.message == WM_INPUT) {
				vlog.note(L"WM_INPUT received").commit();

				auto& lParam = msg.lParam;
				// allocate memory for raw input data
				UINT dwSize{};
				if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER))) {
					p2clog.nox().commit();
					return;
				}
				const auto inputBackingBuffer = std::make_unique<BYTE[]>(dwSize);

				// read in raw input data
				if (const auto size = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, inputBackingBuffer.get(),
					&dwSize, sizeof(RAWINPUTHEADER)); size != dwSize) {
					p2clog.nox().commit();
					return;
				}
				const auto& input = reinterpret_cast<const RAWINPUT&>(*inputBackingBuffer.get());

				// handle raw input data for keyboard device
				if (input.header.dwType == RIM_TYPEKEYBOARD)
				{
					const auto& keyboard = input.data.keyboard;

					vlog.note(std::format(L"KBD| vk:{} msg:{}", keyboard.VKey, keyboard.Message)).commit();

					// check for keycode outside of range of our table
					if (keyboard.VKey >= win::Key::virtualKeyTableSize) {
						vlog.note(L"KBD| vk out of range").commit();
						continue;
					}
					// key presses
					if (keyboard.Message == WM_KEYDOWN || keyboard.Message == WM_SYSKEYDOWN) {
						vlog.note(L"key press").commit();
						// don't handle autorepeat presses
						if (!pressedKeys_[keyboard.VKey]) {
							// mark key as in down state
							pressedKeys_[keyboard.VKey] = true;
							// if key is not modifier
							if (const auto key = win::Key::FromPlatformCode(keyboard.VKey)) {
								if (!key->IsModifierKey()) {
									// gather currently pressed modifiers
									const auto mods = GatherModifiers_();
									// check for matching registered hotkey action
									std::lock_guard lk{ mtx_ };
									if (auto i = registeredHotkeys_.find({ *key, mods });
										i != registeredHotkeys_.cend()) {
										// if match found dispatch action on renderer thread
										vlog.note(L"hotkey dispatching").commit();
										CefPostTask(TID_RENDERER, base::BindOnce(
											&Hotkeys::DispatchHotkey_,
											base::Unretained(this), i->second
										));
									}
								}
							}
						}
						else {
							vlog.note(L"key repeat").commit();
						}
					}
					// key releases
					else if (keyboard.Message == WM_KEYUP || keyboard.Message == WM_SYSKEYUP) {
						vlog.note(L"Key up").commit();
						pressedKeys_[keyboard.VKey] = false;
					}
				}
				else {
					p2clog.warn(L"Unknown raw input type").commit();
				}
			}
		}

		if (!DestroyWindow(static_cast<HWND>(messageWindowHandle_))) {
			p2clog.warn().commit();
		}
		if (!UnregisterClass(MAKEINTATOM(atom), GetModuleHandle(nullptr))) {
			p2clog.warn().commit();
		}

		vlog.note(L"exiting hotkey kernel").commit();
	}
	void Hotkeys::DispatchHotkey_(Action action) const
	{
		std::lock_guard lk{ mtx_ };
		if (Handler_) {
			vlog.note(L"execute handler with hotkey action").commit();
			Handler_(action);
		}
		else {
			p2clog.warn(L"Hotkey handler not set").commit();
		}
	}
	void Hotkeys::SetHandler(std::function<void(Action)> handler)
	{
		std::lock_guard lk{ mtx_ };
		vlog.note(L"Hotkey handler set").commit();
		Handler_ = std::move(handler);
	}
	void Hotkeys::BindAction(Action action, win::Key key, win::ModSet mods, std::function<void(bool)> resultCallback)
	{
		vlog.note(L"Hotkey action binding").commit();
		std::lock_guard lk{ mtx_ };
		// if action is already bound, remove it
		if (const auto i = rn::find_if(registeredHotkeys_, [action](const auto& i) {
			return i.second == action;
		}); i != registeredHotkeys_.cend()) {
			vlog.note(L"Hotkey action binding remove existing action").commit();
			registeredHotkeys_.erase(i);
		}
		// if hotkey combination is already bound, remove it
		registeredHotkeys_.erase({ key, mods });
		// bind action to new hotkey combination
		registeredHotkeys_.emplace(std::piecewise_construct,
			std::forward_as_tuple(key, mods),
			std::forward_as_tuple(action)
		);
		// signal callback success
		resultCallback(true);
	}
	void Hotkeys::ClearAction(Action action, std::function<void(bool)> resultCallback)
	{
		vlog.note(L"Hotkey action clearing").commit();
		std::lock_guard lk{ mtx_ };
		// if action is bound, remove it
		if (const auto i = rn::find_if(registeredHotkeys_, [action](const auto& i) {
			return i.second == action;
			}); i != registeredHotkeys_.cend()) {
			registeredHotkeys_.erase(i);
		}
		else {
			p2clog.warn(L"Attempted to clear unregistered hotkey").nox().commit();
		}
		// signal callback success
		resultCallback(true);
	}
	win::ModSet Hotkeys::GatherModifiers_() const
	{
		win::ModSet mods;
		if (pressedKeys_[VK_SHIFT]) {
			mods = mods | win::Mod::Shift;
		}
		if (pressedKeys_[VK_CONTROL]) {
			mods = mods | win::Mod::Ctrl;
		}
		if (pressedKeys_[VK_MENU]) {
			mods = mods | win::Mod::Alt;
		}
		if (pressedKeys_[VK_LWIN]) {
			mods = mods | win::Mod::Win;
		}
		return mods;
	}


	Hotkeys::Hotkey::Hotkey(win::Key key, win::ModSet mods)
		:
		key{ key },
		mods{ mods }
	{}
	bool Hotkeys::Hotkey::operator<(const Hotkey& rhs) const
	{
		return key == rhs.key ? (mods < rhs.mods) : (key < rhs.key);
	}
	bool Hotkeys::Hotkey::operator==(const Hotkey& rhs) const
	{
		return key == rhs.key && mods == rhs.mods;
	}
}
