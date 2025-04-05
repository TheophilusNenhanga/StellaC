#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "time.h"
#include "../memory.h"

Value timeSecondsFunction_(VM *vm, int argCount, Value *args) {
	return FLOAT_VAL((double)time(NULL));
}

Value timeMillisecondsFunction_(VM *vm, int argCount, Value *args) {
#ifdef _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	ULARGE_INTEGER uli;
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	uint64_t ms = (uli.QuadPart - 116444736000000000ULL) / 10000;
#else
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	uint64_t ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
	return FLOAT_VAL((double)ms);
}

ObjectResult *sleepSecondsFunction(VM *vm, int argCount, Value *args) {
	if (!IS_INT(args[0]) || IS_FLOAT(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Parameter <duration> must be of type 'int' | 'float'.", 54), TYPE,
		                              false));
	}

	double seconds = AS_FLOAT(args[0]);
	if (seconds < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, false));
	}

#ifdef _WIN32
	Sleep((DWORD) (seconds * 1000));
#else
        sleep((unsigned int)seconds);
#endif

	return stellaOk(vm, NIL_VAL);
}

ObjectResult *sleepMillisecondsFunction(VM *vm, int argCount, Value *args) {
	if (!IS_INT(args[0]) || IS_FLOAT(args[0])) {
		return stellaErr(vm, newError(vm, copyString(vm, "Parameter <duration> must be of type 'int' | 'float'.", 54), TYPE,
		                              false));
	}

	double milliseconds = AS_FLOAT(args[0]);
	if (milliseconds < 0) {
		return stellaErr(vm, newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, false));
	}

#ifdef _WIN32
	Sleep((DWORD) milliseconds);
#else
        usleep((useconds_t)(milliseconds * 1000));
#endif

	return stellaOk(vm, NIL_VAL);
}

static time_t getCurrentTime() {
	return time(NULL);
}

Value yearFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_year + 1900);
}

Value monthFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_mon + 1);
}

Value dayFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_mday);
}

Value hourFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_hour);
}

Value minuteFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_min);
}

Value secondFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_sec);
}

Value weekdayFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	// 1 (Monday) - 7 (Sunday)
	int weekday = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday;
	return INT_VAL(weekday);
}

Value dayOfYearFunction_(VM *vm, int argCount, Value *args) {
	time_t t = getCurrentTime();
	struct tm *timeInfo = localtime(&t);
	return INT_VAL(timeInfo->tm_yday + 1);
}
