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

package io.github.xiaotong6666.fusehide.ui

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import kotlinx.coroutines.launch
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.TabRowWithContour
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun FuseHideHomeScreen(
    selectedTab: Int,
    onTabSelected: (Int) -> Unit,
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    debugState: DebugUiState,
    configCallbacks: ConfigCallbacks,
    debugCallbacks: DebugCallbacks,
) {
    val view = LocalView.current
    val pagerState = rememberPagerState(initialPage = selectedTab, pageCount = { 2 })
    val coroutineScope = rememberCoroutineScope()
    val scrollBehavior = MiuixScrollBehavior()

    LaunchedEffect(selectedTab) {
        if (pagerState.currentPage != selectedTab) {
            pagerState.animateScrollToPage(selectedTab)
        }
    }

    LaunchedEffect(pagerState.currentPage) {
        if (pagerState.currentPage != selectedTab) {
            onTabSelected(pagerState.currentPage)
        }
    }

    Scaffold(
        modifier = Modifier.fillMaxSize().nestedScroll(scrollBehavior.nestedScrollConnection),
        topBar = {
            TopAppBar(
                title = stringResource(R.string.app_name),
                color = MiuixTheme.colorScheme.surface,
                titleColor = MiuixTheme.colorScheme.onSurface,
                scrollBehavior = scrollBehavior,
                subtitle = if (selectedTab == 0) {
                    stringResource(R.string.home_subtitle_policy)
                } else {
                    stringResource(R.string.home_subtitle_probe)
                },
                subtitleColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                bottomContent = {
                    TabRowWithContour(
                        tabs = listOf(
                            stringResource(R.string.tab_policy),
                            stringResource(R.string.tab_probe),
                        ),
                        selectedTabIndex = selectedTab,
                        onTabSelected = {
                            view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                            onTabSelected(it)
                            coroutineScope.launch { pagerState.animateScrollToPage(it) }
                        },
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                    )
                },
            )
        },
    ) { paddingValues ->
        HorizontalPager(
            state = pagerState,
            modifier = Modifier.fillMaxSize(),
        ) { page ->
            when (page) {
                0 -> ConfigScreen(
                    hookStatus = hookStatus,
                    state = configState,
                    callbacks = configCallbacks,
                    contentPadding = paddingValues,
                )

                else -> DebugScreen(
                    hookStatus = hookStatus,
                    state = debugState,
                    callbacks = debugCallbacks,
                    contentPadding = paddingValues,
                )
            }
        }
    }
}

// For Android Studio preview compose interface.
@Preview(showBackground = true, device = "id:pixel_9_pro")
@Composable
private fun PreviewFuseHideHomeScreen() {
    io.github.xiaotong6666.fusehide.ui.theme.fuseHideTheme {
        FuseHideHomeScreen(
            selectedTab = 0, // 0 预览配置页，改成 1 预览测试页
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
                enableHideAllRootEntries = true,
                hideAllRootEntriesExemptionsText = "Android\nDCIM\nDocument\nDownload\nMovies\nPictures",
                hiddenTargetsText = "xinhao\nMT2",
                hiddenPackagesText = "com.eltavine.duckdetector\nio.github.xiaotong6666.fusehide\nio.github.a13e300.fusefixer",
            ),
            debugState = DebugUiState(
                pathText = "/storage/emulated/0/Android",
                pathText2 = "",
                outputText = "Stat /storage/emulated/0/Android -> OK",
            ),
            configCallbacks = ConfigCallbacks(
                onStatusClick = {},
                onEnableHideAllRootEntriesChanged = {},
                onHideAllRootEntriesExemptionsChanged = {},
                onHiddenTargetsChanged = {},
                onHiddenPackagesChanged = {},
                onSaveConfigClick = {},
                onApplyConfigClick = {},
                onResetConfigClick = {},
                onRefreshAppliedConfigClick = {},
            ),
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
        )
    }
}
