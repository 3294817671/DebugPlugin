#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <list>
#include <string>
#include <ntifs.h>
#include <ntimage.h>
#include <ntintsafe.h>
#include "common.h"
#include "ia32_type.h"
#include <intrin.h>
// ����Ҫ�� VC++Ŀ¼�İ���Ŀ¼    ����� $(VC_IncludePath)


#define Log(format, ...) \
  DbgPrintEx(0,0, (format), __VA_ARGS__)