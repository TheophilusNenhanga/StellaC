
#ifndef IO_H
#define IO_H

#include "../value.h"
#include "../object.h"

NativeReturn printNative(VM* vm,int argCount, Value *args);
NativeReturn printlnNative(VM* vm,int argCount, Value *args);

#endif // IO_H
