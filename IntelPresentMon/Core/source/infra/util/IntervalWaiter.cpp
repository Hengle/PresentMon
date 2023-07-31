#include "IntervalWaiter.h"
#include <Core/source/win/WinAPI.h>
#include <Core/source/infra/log/Logging.h>

namespace p2c::infra::util
{
	IntervalWaiter::IntervalWaiter(double interval)
		:
		interval_{ interval }
	{
		waitableTimerHandle_ = CreateWaitableTimerExW(
			nullptr,
			nullptr,
			CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
			TIMER_ALL_ACCESS);
		if (!waitableTimerHandle_) {
			p2clog.note(L"Failed creating high resolution timer").hr().commit();
		}
	}

	void IntervalWaiter::SetInterval(double interval)
	{
		interval_ = interval;
	}

	void IntervalWaiter::Wait()
	{
		const auto waitTimeSeconds = [=, this] {
			const auto t = timer_.Peek();
			auto targetCandidate = lastTargetTime_ + interval_;
			// if we are on-time
			if (t <= targetCandidate) {
				lastTargetTime_ = targetCandidate;
				return targetCandidate - t;
			}
			// if we are late, reset target to NOW and do not wait
			lastTargetTime_ = t;
			return 0.;
		}();

		if (waitTimeSeconds == 0.) return;

		// wait slightly (waitBuffer_ seconds) shorter than required to compensate for overwait error
		const LARGE_INTEGER waitTime100ns{
			.QuadPart = -LONGLONG(double(waitTimeSeconds - waitBuffer_) * 10'000'000.)
		};
		if (!SetWaitableTimerEx(
			waitableTimerHandle_,
			&waitTime100ns,
			0,
			nullptr,
			nullptr,
			nullptr,
			0
		)) {
			p2clog.note(L"Failed setting high resolution timer").hr().commit();
		}
		if (auto code = WaitForSingleObject(waitableTimerHandle_, INFINITE)) {
			p2clog.note(std::format(L"Failed waiting on high resolution timer CODE:{:8x}", code)).hr().commit();
		}
		// spin wait remaining time to obtain more accurate wait
		while (timer_.Peek() < lastTargetTime_) {
			std::this_thread::yield();
		}
	}

	IntervalWaiter::~IntervalWaiter()
	{
		if (!CloseHandle(waitableTimerHandle_)) {
			p2clog.note(L"Failed closing high resolution timer").nox().notrace().hr().commit();
		}
	}
}
