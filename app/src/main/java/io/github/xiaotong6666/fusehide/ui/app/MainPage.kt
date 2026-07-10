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

@file:Suppress("ktlint:standard:function-naming")

package io.github.xiaotong6666.fusehide.ui.app

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.outlined.Home
import androidx.compose.material.icons.outlined.Search
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.Tune
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.core.model.DebugCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.DebugUiState
import io.github.xiaotong6666.fusehide.ui.core.model.HideConfigDiff
import io.github.xiaotong6666.fusehide.ui.core.model.HomeCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.HookStatusUiState
import io.github.xiaotong6666.fusehide.ui.core.model.SettingsCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.SettingsUiState
import io.github.xiaotong6666.fusehide.ui.feature.config.ConfigPage
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.AppListViewModel
import io.github.xiaotong6666.fusehide.ui.feature.debug.DebugPage
import io.github.xiaotong6666.fusehide.ui.feature.home.HomePage
import io.github.xiaotong6666.fusehide.ui.feature.settings.SettingsPage
import io.github.xiaotong6666.uihelper.chrome.AdaptiveNavigationShell
import io.github.xiaotong6666.uihelper.chrome.NavigationShellItem
import io.github.xiaotong6666.uihelper.mode.UiMode

@Composable
fun MainPage(
    selectedTab: Int,
    onTabSelected: (Int) -> Unit,
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    debugState: DebugUiState,
    homeCallbacks: HomeCallbacks,
    configCallbacks: ConfigCallbacks,
    appListViewModel: AppListViewModel,
    onOpenGlobalConfig: () -> Unit,
    onOpenAppConfig: (String) -> Unit,
    debugCallbacks: DebugCallbacks,
    settingsState: SettingsUiState,
    settingsCallbacks: SettingsCallbacks,
) {
    val pageItems = listOf(
        NavigationShellItem(
            title = stringResource(R.string.nav_home),
            icon = Icons.Outlined.Home,
            selectedIcon = Icons.Filled.Home,
        ),
        NavigationShellItem(
            title = stringResource(R.string.nav_config),
            icon = Icons.Outlined.Tune,
            selectedIcon = Icons.Filled.Tune,
        ),
        NavigationShellItem(
            title = stringResource(R.string.nav_probe),
            icon = Icons.Outlined.Search,
            selectedIcon = Icons.Filled.Search,
        ),
        NavigationShellItem(
            title = stringResource(R.string.nav_settings),
            icon = Icons.Outlined.Settings,
            selectedIcon = Icons.Filled.Settings,
        ),
    )

    AdaptiveNavigationShell(
        items = pageItems,
        selectedIndex = selectedTab,
        onSelectedIndexChange = onTabSelected,
    ) { page, contentPadding, isCurrentPage, pageModifier ->
        when (page) {
            0 -> HomePage(
                hookStatus = hookStatus,
                configState = configState,
                callbacks = homeCallbacks,
                contentPadding = contentPadding,
                isCurrentPage = isCurrentPage,
                modifier = pageModifier,
            )

            1 -> ConfigPage(
                hookStatus = hookStatus,
                state = configState,
                callbacks = configCallbacks,
                contentPadding = contentPadding,
                isCurrentPage = isCurrentPage,
                modifier = pageModifier,
                appListViewModel = appListViewModel,
                onOpenGlobalConfig = onOpenGlobalConfig,
                onOpenAppConfig = onOpenAppConfig,
            )

            2 -> DebugPage(
                state = debugState,
                callbacks = debugCallbacks,
                contentPadding = contentPadding,
                isCurrentPage = isCurrentPage,
                modifier = pageModifier,
            )

            else -> SettingsPage(
                state = settingsState,
                callbacks = settingsCallbacks,
                contentPadding = contentPadding,
                isCurrentPage = isCurrentPage,
                modifier = pageModifier,
            )
        }
    }
}

@Preview(showBackground = true, device = "id:pixel_9_pro")
@Composable
private fun PreviewMainPage() {
    io.github.xiaotong6666.fusehide.ui.theme.FuseHideTheme {
        MainPage(
            selectedTab = 0,
            onTabSelected = {},
            hookStatus = HookStatusUiState(
                infoText = "Kernel: 6.1.118\nDevice: Fuxi\nSDK: 3600000",
                statusText = "Hooked: com.example.app (1234)",
                isHooked = true,
                hookedPackage = "com.example.app",
                hookedPid = 1234,
                hookCheckCompleted = true,
            ),
            configState = ConfigUiState(
                configStatusText = "The saved hidden configuration has been loaded.",
                lastAckTokenText = "-",
                lastAckResultText = "-",
                lastApplyTimeText = "-",
                draftVsAppliedDiff = HideConfigDiff(
                    hasDifferences = false,
                    summary = "None",
                    details = "",
                ),
                appliedConfigSnapshotText = "Current native config snapshot...",
                highlightConfigResults = false,
                configResultsScrollToken = 0,
                currentHideConfig = io.github.xiaotong6666.fusehide.config.HideConfigDefaults.value,
            ),
            debugState = DebugUiState(
                pathText = "/storage/emulated/0/Android",
                pathText2 = "",
                outputText = "Stat /storage/emulated/0/Android -> OK",
            ),
            homeCallbacks = HomeCallbacks(
                onStatusClick = {},
            ),
            configCallbacks = ConfigCallbacks(
                onStatusClick = {},
                onConfigUpdate = {},
                onApplyConfigClick = {},
                onResetConfigClick = {},
            ),
            appListViewModel = androidx.lifecycle.viewmodel.compose.viewModel(),
            onOpenGlobalConfig = {},
            onOpenAppConfig = {},
            debugCallbacks = DebugCallbacks(
                onStatusClick = {},
                onPathChanged = {},
                onPath2Changed = {},
                onStatClick = {},
                onAccessClick = {},
                onListClick = {},
                onOpenClick = {},
                onGetConClick = {},
                onCreateClick = {},
                onMkdirClick = {},
                onMoveClick = {},
                onRmdirClick = {},
                onUnlinkClick = {},
                onAllPkgClick = {},
                onInsertZwjClick = {},
                onClearClick = {},
                onResetClick = {},
                onCopyAllClick = {},
                onSelfDataClick = {},
            ),
            settingsState = SettingsUiState(uiMode = UiMode.Miuix),
            settingsCallbacks = SettingsCallbacks(onToggleUiMode = {}),
        )
    }
}
