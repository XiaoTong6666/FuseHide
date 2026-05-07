// Copyright (C) 2026 XiaoTong6666
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "dirent_filter.hpp"

namespace fusehide {

struct ReplyBufFilterContext {
    uint32_t filterUid = 0;
    uint64_t filterIno = 0;
    bool filterPlainReaddir = false;
    bool filterPostfilterReaddir = false;
    bool filterReaddirplus = false;
    bool requireParentMatch = true;
    bool enableAutoFallback = true;
};

struct ReplyBufFilterResult {
    const char* data = nullptr;
    size_t size = 0;
    const char* mode = nullptr;
    size_t removedCount = 0;
};

ReplyBufFilterResult FilterReplyBufPayload(const char* buf, size_t size,
                                           const ReplyBufFilterContext& context,
                                           std::vector<char>* filteredStorage);

}  // namespace fusehide
