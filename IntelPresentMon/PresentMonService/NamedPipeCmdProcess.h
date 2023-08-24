// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
#pragma once

#include "PresentMon.h"
#include "..\PresentMonUtils\MemBuffer.h"
#include "..\PresentMonUtils\PresentMonNamedPipe.h"

void ProcessRequests(PresentMon* pm, MemBuffer* rqstBuf, MemBuffer* rspBuf);