// Copyright (C) 2026 XiaoTong6666
// SPDX-License-Identifier: Apache-2.0

#include "lsposed_scope.hpp"

#include <dlfcn.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace fusehide::zygisk {
namespace {

struct sqlite3;
using Open = int (*)(const char*, sqlite3**, int, const char*);
using Exec = int (*)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**);
using Close = int (*)(sqlite3*);
using BusyTimeout = int (*)(sqlite3*, int);

}  // namespace

LsposedScope QueryLsposedScope(const char* sqliteLibrary, const char* database, int userId,
                               bool googleMediaProvider) {
    if (userId < 0) {
        return LsposedScope::kUnknown;
    }
    if (access(database, F_OK) != 0) {
        return errno == ENOENT ? LsposedScope::kDisabled : LsposedScope::kUnknown;
    }
    void* library = dlopen(sqliteLibrary, RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        return LsposedScope::kUnknown;
    }
    const auto open = reinterpret_cast<Open>(dlsym(library, "sqlite3_open_v2"));
    const auto exec = reinterpret_cast<Exec>(dlsym(library, "sqlite3_exec"));
    const auto close = reinterpret_cast<Close>(dlsym(library, "sqlite3_close"));
    const auto busyTimeout = reinterpret_cast<BusyTimeout>(dlsym(library, "sqlite3_busy_timeout"));
    LsposedScope result = LsposedScope::kUnknown;
    sqlite3* db = nullptr;
    constexpr int kOpenReadOnly = 0x00000001;
    if (open != nullptr && exec != nullptr && close != nullptr && busyTimeout != nullptr &&
        open(database, &db, kOpenReadOnly, nullptr) == 0) {
        busyTimeout(db, 100);
        constexpr const char* kQueries[] = {
            "SELECT 1 FROM modules m JOIN scope s ON m.mid=s.mid "
            "WHERE m.module_pkg_name='io.github.xiaotong6666.fusehide' AND m.enabled=1 "
            "AND s.app_pkg_name='%s' AND s.user_id=%d LIMIT 1;",
            "SELECT 1 FROM modules_state ms JOIN scope s ON "
            "ms.module_pkg_name=s.module_pkg_name AND ms.user_id=s.user_id "
            "WHERE ms.module_pkg_name='io.github.xiaotong6666.fusehide' AND ms.enabled=1 "
            "AND s.app_pkg_name='%s' AND ms.user_id=%d LIMIT 1;",
        };
        const char* target = googleMediaProvider ? "com.google.android.providers.media.module"
                                                 : "com.android.providers.media.module";
        for (const char* query : kQueries) {
            char sql[512];
            snprintf(sql, sizeof(sql), query, target, userId);
            bool enabled = false;
            const int status = exec(
                db, sql,
                [](void* opaque, int count, char** values, char**) -> int {
                    if (count > 0 && values[0] != nullptr && strcmp(values[0], "1") == 0) {
                        *static_cast<bool*>(opaque) = true;
                    }
                    return 0;
                },
                &enabled, nullptr);
            if (status == 0) {
                result = enabled ? LsposedScope::kEnabled : LsposedScope::kDisabled;
                break;
            }
        }
    }
    if (db != nullptr && close != nullptr) {
        close(db);
    }
    dlclose(library);
    return result;
}

}  // namespace fusehide::zygisk
