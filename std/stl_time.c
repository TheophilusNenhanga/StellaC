#include <time.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "stl_time.h"
#include "../memory.h"

ObjectResult* _time_s(VM* vm, int argCount, Value* args) {
	nativeReturn.values[0] = NUMBER_VAL((double)time(NULL));
	return nativeReturn;
}

ObjectResult* _time_ms(VM *vm, int argCount, Value *args) {
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
	nativeReturn.values[0] = NUMBER_VAL((double)ms);
	return nativeReturn;
}

ObjectResult* _sleep_s(VM *vm, int argCount, Value *args) {
		if (!IS_NUMBER(args[0])) {
			nativeReturn.values[0] = NIL_VAL;
			nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Parameter <duration> must be of type 'number'.", 46), TYPE, false));
			return nativeReturn;
		}

    double seconds = AS_NUMBER(args[0]);
    if (seconds < 0) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, false));
        return nativeReturn;
    }

    #ifdef _WIN32
        Sleep((DWORD)(seconds * 1000));
    #else
        sleep((unsigned int)seconds);
    #endif

    nativeReturn.values[0] = NIL_VAL;
    nativeReturn.values[1] = NIL_VAL;
    return nativeReturn;
}

ObjectResult* _sleep_ms(VM *vm, int argCount, Value *args) {
	if (!IS_NUMBER(args[0])) {
		nativeReturn.values[0] = NIL_VAL;
		nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Parameter <duration> must be of type 'number'.", 46), TYPE, false));
		return nativeReturn;
	}

    double milliseconds = AS_NUMBER(args[0]);
    if (milliseconds < 0) {
        nativeReturn.values[0] = NIL_VAL;
        nativeReturn.values[1] = OBJECT_VAL(newError(vm, copyString(vm, "Sleep duration cannot be negative.", 34), VALUE, false));
        return nativeReturn;
    }

    #ifdef _WIN32
        Sleep((DWORD)milliseconds);
    #else
        usleep((useconds_t)(milliseconds * 1000));
    #endif

    nativeReturn.values[0] = NIL_VAL;
    nativeReturn.values[1] = NIL_VAL;
    return nativeReturn;
}

static time_t getCurrentTime() {
    return time(NULL);
}

ObjectResult* _year(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_year + 1900);
    return nativeReturn;
}

ObjectResult* _month(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_mon + 1);
    return nativeReturn;
}

ObjectResult* _day(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_mday);
    return nativeReturn;
}

ObjectResult* _hour(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_hour);
    return nativeReturn;
}

ObjectResult* _minute(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_min);
    return nativeReturn;
}

ObjectResult* _second(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_sec);
    return nativeReturn;
}

ObjectResult* _weekday(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    // 1 (Monday) - 7 (Sunday)
    int weekday = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday;
    nativeReturn.values[0] = NUMBER_VAL(weekday);
    return nativeReturn;
}

ObjectResult* _day_of_year(VM *vm, int argCount, Value *args) {
    time_t t = getCurrentTime();
    struct tm *timeInfo = localtime(&t);
    nativeReturn.values[0] = NUMBER_VAL(timeInfo->tm_yday + 1);
    return nativeReturn;
}
