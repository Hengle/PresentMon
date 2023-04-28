// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <map>
#include <thread>
#include <semaphore>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <Core/source/win/Key.h>
#include <Core/source/win/ModSet.h>
#include "../Action.h"

namespace p2c::client::util
{
	// IDEA: windows-side action ids are separate and mapped to application action ids
	// then we can try to register a new combination without first deleting the old
	// retaining it for a strong exception guarantee (race condition when revert register fails)
	// this will also allow: multiple mappings for the same action (dubious utility)

	class Hotkeys
	{
	public:
		Hotkeys();
		~Hotkeys();
		void BindAction(Action action, win::Key key, win::ModSet mods, std::function<void(bool)> resultCallback);
		void ClearAction(Action action, std::function<void(bool)> resultCallback);
		void SetHandler(std::function<void(Action)> handler);
	private:
		// types
		struct Hotkey
		{
		public:
			Hotkey(Action action, win::Key key, win::ModSet mods);
			Hotkey(Hotkey&& donor) = delete;
			Hotkey(const Hotkey&) = delete;
			Hotkey& operator=(Hotkey&& donor) = delete;
			Hotkey& operator=(const Hotkey&) = delete;
			~Hotkey();
			bool CombinationMatches(win::Key key, win::ModSet mods) const;
			void ChangeCombination(win::Key key, win::ModSet mods);
		private:
			void Unregister_();
			void Register_(Action action, win::Key key, win::ModSet mods);
			Action action;
			win::Key key;
			win::ModSet mods;
		};
		// functions
		void DispatchHotkey_(Action action) const;
		void BindAction_(Action action, win::Key key, win::ModSet mods);
		void ClearAction_(Action action);
		void Kernel_();
		// data
		// for constructor wait
		std::binary_semaphore sem{ 0 };
		// control access to shared memory for handler function
		mutable std::mutex mtx;
		std::function<void(Action)> Handle_;
		uint32_t tid;
		std::map<Action, Hotkey> registeredHotkeys;
		std::jthread thread;
	};
}