/*
 * Copyright (C) 2026 XiaoTong6666
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import androidx.compose.runtime.Immutable

data class AppInfo(
    val packageName: String,
    val label: String,
    val packageInfo: PackageInfo,
    val applicationInfo: ApplicationInfo,
    val uid: Int,
)

@Immutable
data class GroupedApps(
    val uid: Int,
    val primary: AppInfo,
    val apps: List<AppInfo>,
    val ownerName: String? = null,
    val matchedPackageNames: Set<String> = emptySet(),
    val isHidden: Boolean = false,
)

fun ownerNameForApps(
    apps: List<AppInfo>,
    packageManager: PackageManager,
    fallback: String,
): String {
    val labeledApp = apps.firstOrNull { it.packageInfo.sharedUserLabel != 0 }
    if (labeledApp != null) {
        val label = runCatching {
            packageManager.getText(
                labeledApp.packageName,
                labeledApp.packageInfo.sharedUserLabel,
                labeledApp.applicationInfo,
            )
        }.getOrNull()?.toString().orEmpty()
        if (label.isNotBlank()) {
            return label
        }
    }

    val sharedUserId = apps.firstOrNull {
        !it.packageInfo.sharedUserId.isNullOrEmpty()
    }?.packageInfo?.sharedUserId
    if (!sharedUserId.isNullOrEmpty()) {
        return sharedUserId
    }

    return apps.firstOrNull()?.label?.ifBlank { fallback } ?: fallback
}
