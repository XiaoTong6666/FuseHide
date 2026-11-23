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

package io.github.xiaotong6666.fusehide.ui.app

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager

/**
 * Only the launcher alias is toggled; MainActivity remains available through
 * the Xposed MODULE_SETTINGS intent and an explicit component launch.
 */
object LauncherIconSettings {
    private fun launcherAlias(context: Context) = ComponentName(
        context.packageName,
        "${context.packageName}.ui.app.LauncherAlias",
    )

    fun isHidden(context: Context): Boolean = when (
        context.packageManager.getComponentEnabledSetting(launcherAlias(context))
    ) {
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED,
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED_USER,
        PackageManager.COMPONENT_ENABLED_STATE_DISABLED_UNTIL_USED,
        -> true

        else -> false
    }

    fun setHidden(context: Context, hidden: Boolean): Boolean {
        context.packageManager.setComponentEnabledSetting(
            launcherAlias(context),
            if (hidden) {
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED
            } else {
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT
            },
            PackageManager.DONT_KILL_APP,
        )
        return isHidden(context) == hidden
    }
}
