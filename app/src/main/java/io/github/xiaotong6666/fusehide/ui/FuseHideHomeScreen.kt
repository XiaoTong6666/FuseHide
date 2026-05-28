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
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.PrimaryTabRow
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Tab
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
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
import top.yukonga.miuix.kmp.basic.TabRowWithContour
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun ThemeToggleButton(onClick: () -> Unit) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> {
            Text(
                text = "M3",
                color = MiuixTheme.colorScheme.onSurface,
                style = MiuixTheme.textStyles.button,
                modifier = Modifier
                    .clickable(onClick = onClick)
                    .padding(horizontal = 12.dp, vertical = 8.dp),
            )
        }

        UiMode.Material -> {
            TextButton(onClick = onClick) {
                Text(text = "MIUI", color = MaterialTheme.colorScheme.onSurface)
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FuseHideHomeScreen(
    selectedTab: Int,
    onTabSelected: (Int) -> Unit,
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    debugState: DebugUiState,
    configCallbacks: ConfigCallbacks,
    debugCallbacks: DebugCallbacks,
    onToggleUiMode: () -> Unit = {},
) {
    val view = LocalView.current
    val pagerState = rememberPagerState(initialPage = selectedTab, pageCount = { 2 })
    val coroutineScope = rememberCoroutineScope()

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

    when (LocalUiMode.current) {
        UiMode.Miuix -> {
            val scrollBehavior = MiuixScrollBehavior()
            top.yukonga.miuix.kmp.basic.Scaffold(
                modifier = Modifier.fillMaxSize().nestedScroll(scrollBehavior.nestedScrollConnection),
                topBar = {
                    top.yukonga.miuix.kmp.basic.TopAppBar(
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
                        actions = { ThemeToggleButton(onClick = onToggleUiMode) },
                        bottomContent = {
                            TabRowWithContour(
                                tabs = listOf(stringResource(R.string.tab_policy), stringResource(R.string.tab_probe)),
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
                HorizontalPager(state = pagerState, modifier = Modifier.fillMaxSize()) { page ->
                    when (page) {
                        0 -> ConfigScreen(hookStatus = hookStatus, state = configState, callbacks = configCallbacks, contentPadding = paddingValues)
                        else -> DebugScreen(hookStatus = hookStatus, state = debugState, callbacks = debugCallbacks, contentPadding = paddingValues)
                    }
                }
            }
        }

        UiMode.Material -> {
            val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior()
            val tabs = listOf(stringResource(R.string.tab_policy), stringResource(R.string.tab_probe))
            Scaffold(
                modifier = Modifier.fillMaxSize().nestedScroll(scrollBehavior.nestedScrollConnection),
                topBar = {
                    Column {
                        TopAppBar(
                            title = {
                                Column {
                                    Text(text = stringResource(R.string.app_name), style = MaterialTheme.typography.headlineSmall)
                                    Text(
                                        text = if (selectedTab == 0) stringResource(R.string.home_subtitle_policy) else stringResource(R.string.home_subtitle_probe),
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                            },
                            actions = { ThemeToggleButton(onClick = onToggleUiMode) },
                            colors = TopAppBarDefaults.topAppBarColors(
                                containerColor = MaterialTheme.colorScheme.surface,
                                scrolledContainerColor = MaterialTheme.colorScheme.surface,
                            ),
                            scrollBehavior = scrollBehavior,
                        )
                        PrimaryTabRow(
                            selectedTabIndex = selectedTab,
                            containerColor = MaterialTheme.colorScheme.surface,
                            contentColor = MaterialTheme.colorScheme.onSurface,
                            divider = {},
                        ) {
                            tabs.forEachIndexed { index, title ->
                                Tab(
                                    selected = selectedTab == index,
                                    onClick = {
                                        view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                                        onTabSelected(index)
                                        coroutineScope.launch { pagerState.animateScrollToPage(index) }
                                    },
                                    text = { Text(text = title, style = MaterialTheme.typography.labelLarge) },
                                )
                            }
                        }
                    }
                },
            ) { paddingValues ->
                HorizontalPager(state = pagerState, modifier = Modifier.fillMaxSize()) { page ->
                    when (page) {
                        0 -> ConfigScreen(hookStatus = hookStatus, state = configState, callbacks = configCallbacks, contentPadding = paddingValues)
                        else -> DebugScreen(hookStatus = hookStatus, state = debugState, callbacks = debugCallbacks, contentPadding = paddingValues)
                    }
                }
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
                enableHideAllRootEntries = true,
                hideAllRootEntriesExemptionsText = "Android\nDCIM\nDocument\nDownload\nMovies\nPictures",
                hiddenTargetsText = "su\ndaemonsu\n\n[io.github.xiaotong6666.fusehide]\nxinhao\n\n[com.eltavine.duckdetector]\nMT2\nxinhao",
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
