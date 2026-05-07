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

#include "reply_buf_filter.hpp"

namespace fusehide {

ReplyBufFilterResult FilterReplyBufPayload(const char* buf, size_t size,
                                           const ReplyBufFilterContext& context,
                                           std::vector<char>* filteredStorage) {
    ReplyBufFilterResult result{.data = buf, .size = size, .mode = nullptr, .removedCount = 0};
    if (buf == nullptr || size == 0 || filteredStorage == nullptr ||
        !HiddenPathPolicy::IsTestHiddenUid(context.filterUid)) {
        return result;
    }

    size_t removedCount = 0;
    if (context.filterPlainReaddir) {
        if (DirentFilter::BuildFilteredDirentPayload(buf, size, context.filterUid,
                                                     context.filterIno, filteredStorage,
                                                     &removedCount, context.requireParentMatch)) {
            return {filteredStorage->data(), filteredStorage->size(), "readdir", removedCount};
        }
    } else if (context.filterReaddirplus) {
        if (DirentFilter::BuildFilteredDirentplusPayload(
                buf, size, context.filterUid, context.filterIno, filteredStorage, &removedCount,
                context.requireParentMatch)) {
            return {filteredStorage->data(), filteredStorage->size(), "readdirplus", removedCount};
        }
    } else if (context.filterPostfilterReaddir && size >= sizeof(fuse_read_out)) {
        const auto* readOut = reinterpret_cast<const fuse_read_out*>(buf);
        const size_t payloadSize = std::min<size_t>(readOut->size, size - sizeof(fuse_read_out));
        std::vector<char> filteredPayload;
        if (DirentFilter::BuildFilteredDirentPayload(
                buf + sizeof(fuse_read_out), payloadSize, context.filterUid, context.filterIno,
                &filteredPayload, &removedCount, context.requireParentMatch)) {
            fuse_read_out patched = *readOut;
            patched.size = static_cast<uint32_t>(filteredPayload.size());
            filteredStorage->resize(sizeof(patched) + filteredPayload.size());
            std::memcpy(filteredStorage->data(), &patched, sizeof(patched));
            std::memcpy(filteredStorage->data() + sizeof(patched), filteredPayload.data(),
                        filteredPayload.size());
            return {filteredStorage->data(), filteredStorage->size(), "readdir_postfilter",
                    removedCount};
        }
    }

    if (context.enableAutoFallback) {
        if (DirentFilter::BuildFilteredDirentplusPayload(buf, size, context.filterUid, 0,
                                                         filteredStorage, &removedCount, false)) {
            return {filteredStorage->data(), filteredStorage->size(), "auto_direntplus",
                    removedCount};
        }
        if (DirentFilter::BuildFilteredDirentPayload(buf, size, context.filterUid, 0,
                                                     filteredStorage, &removedCount, false)) {
            return {filteredStorage->data(), filteredStorage->size(), "auto_dirent", removedCount};
        }
        if (size >= sizeof(fuse_read_out)) {
            const auto* readOut = reinterpret_cast<const fuse_read_out*>(buf);
            const size_t payloadSize =
                std::min<size_t>(readOut->size, size - sizeof(fuse_read_out));
            std::vector<char> filteredPayload;
            if (DirentFilter::BuildFilteredDirentPayload(buf + sizeof(fuse_read_out), payloadSize,
                                                         context.filterUid, 0, &filteredPayload,
                                                         &removedCount, false)) {
                fuse_read_out patched = *readOut;
                patched.size = static_cast<uint32_t>(filteredPayload.size());
                filteredStorage->resize(sizeof(patched) + filteredPayload.size());
                std::memcpy(filteredStorage->data(), &patched, sizeof(patched));
                std::memcpy(filteredStorage->data() + sizeof(patched), filteredPayload.data(),
                            filteredPayload.size());
                return {filteredStorage->data(), filteredStorage->size(), "auto_read_out_dirent",
                        removedCount};
            }
        }
    }
    return result;
}

}  // namespace fusehide
