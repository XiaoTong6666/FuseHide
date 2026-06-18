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
import androidx.compose.foundation.MutatePriority
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Home
import androidx.compose.material.icons.outlined.Search
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.Tune
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import io.github.xiaotong6666.fusehide.R
import kotlinx.coroutines.launch
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import kotlin.math.abs
import kotlin.math.roundToInt
import top.yukonga.miuix.kmp.basic.NavigationBar as MiuixNavigationBar
import top.yukonga.miuix.kmp.basic.NavigationBarItem as MiuixNavigationBarItem

private data class MainDestinationSpec(
    val destination: MainDestination,
    val title: String,
    val subtitle: String,
    val icon: ImageVector,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainPage(
    selectedTab: Int,
    onTabSelected: (Int) -> Unit,
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    debugState: DebugUiState,
    homeCallbacks: HomeCallbacks,
    configCallbacks: ConfigCallbacks,
    debugCallbacks: DebugCallbacks,
    settingsState: SettingsUiState,
    settingsCallbacks: SettingsCallbacks,
) {
    val view = LocalView.current
    val pageSpecs = remember {
        listOf(
            MainDestinationSpec(MainDestination.Home, "", "", Icons.Outlined.Home),
            MainDestinationSpec(MainDestination.Config, "", "", Icons.Outlined.Tune),
            MainDestinationSpec(MainDestination.Probe, "", "", Icons.Outlined.Search),
            MainDestinationSpec(MainDestination.Settings, "", "", Icons.Outlined.Settings),
        )
    }.map { spec ->
        spec.copy(
            title = when (spec.destination) {
                MainDestination.Home -> stringResource(R.string.nav_home)
                MainDestination.Config -> stringResource(R.string.nav_config)
                MainDestination.Probe -> stringResource(R.string.nav_probe)
                MainDestination.Settings -> stringResource(R.string.nav_settings)
            },
            subtitle = when (spec.destination) {
                MainDestination.Home -> stringResource(R.string.home_subtitle_runtime)
                MainDestination.Config -> stringResource(R.string.home_subtitle_policy)
                MainDestination.Probe -> stringResource(R.string.home_subtitle_probe)
                MainDestination.Settings -> stringResource(R.string.home_subtitle_settings)
            },
        )
    }
    val pagerState = rememberPagerState(initialPage = selectedTab, pageCount = { pageSpecs.size })
    val scope = rememberCoroutineScope()
    val settledPage = pagerState.settledPage
    val scrollingPageIndex = (pagerState.currentPage + pagerState.currentPageOffsetFraction)
        .roundToInt()
        .coerceIn(0, pageSpecs.lastIndex)
    val activePageIndex = when {
        pagerState.isScrollInProgress -> scrollingPageIndex
        else -> settledPage.coerceIn(0, pageSpecs.lastIndex)
    }
    val activePage = pageSpecs[activePageIndex]
    val miuixScrollBehavior = MiuixScrollBehavior()
    val onPageSelected: (Int) -> Unit = { index ->
        if (pagerState.isScrollInProgress || settledPage != index) {
            view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
            scope.launch { pagerState.springScrollToPage(index) }
        }
    }

    LaunchedEffect(settledPage) {
        if (selectedTab != settledPage) {
            onTabSelected(settledPage)
        }
    }

    LaunchedEffect(selectedTab) {
        val coercedTarget = selectedTab.coerceIn(0, pageSpecs.lastIndex)
        if (!pagerState.isScrollInProgress && coercedTarget != settledPage) {
            if (pagerState.currentPage != coercedTarget) {
                pagerState.springScrollToPage(coercedTarget)
            }
        }
    }

    when (LocalUiMode.current) {
        UiMode.Miuix -> {
            top.yukonga.miuix.kmp.basic.Scaffold(
                modifier = Modifier.fillMaxSize(),
                topBar = {
                    top.yukonga.miuix.kmp.basic.TopAppBar(
                        title = stringResource(R.string.app_name),
                        color = MiuixTheme.colorScheme.surface,
                        titleColor = MiuixTheme.colorScheme.onSurface,
                        subtitle = activePage.subtitle,
                        subtitleColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                        scrollBehavior = miuixScrollBehavior,
                    )
                },
                bottomBar = {
                    MiuixNavigationBar {
                        pageSpecs.forEachIndexed { index, page ->
                            MiuixNavigationBarItem(
                                selected = activePageIndex == index,
                                onClick = { onPageSelected(index) },
                                icon = page.icon,
                                label = page.title,
                            )
                        }
                    }
                },
            ) { paddingValues ->
                HorizontalPager(
                    state = pagerState,
                    modifier = Modifier.fillMaxSize(),
                    overscrollEffect = null,
                ) { page ->
                    val scrollModifier = Modifier
                        .then(if (page == pagerState.currentPage) Modifier.overScrollVertical() else Modifier)
                        .nestedScroll(miuixScrollBehavior.nestedScrollConnection)

                    when (page) {
                        0 -> HomePage(hookStatus = hookStatus, configState = configState, callbacks = homeCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage, scrollModifier = scrollModifier, title = stringResource(R.string.app_name), subtitle = activePage.subtitle)
                        1 -> ConfigPage(hookStatus = hookStatus, state = configState, callbacks = configCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage, scrollModifier = scrollModifier, title = stringResource(R.string.app_name), subtitle = activePage.subtitle)
                        2 -> DebugPage(state = debugState, callbacks = debugCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage, scrollModifier = scrollModifier, title = stringResource(R.string.app_name), subtitle = activePage.subtitle)
                        else -> SettingsPage(state = settingsState, callbacks = settingsCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage, scrollModifier = scrollModifier, title = stringResource(R.string.app_name), subtitle = activePage.subtitle)
                    }
                }
            }
        }

        UiMode.Material -> {
            val scrollBehavior = TopAppBarDefaults.pinnedScrollBehavior()
            Scaffold(
                modifier = Modifier.fillMaxSize().nestedScroll(scrollBehavior.nestedScrollConnection),
                topBar = {
                    Column {
                        TopAppBar(
                            title = {
                                Column {
                                    Text(text = stringResource(R.string.app_name), style = MaterialTheme.typography.headlineSmall)
                                    Text(
                                        text = activePage.subtitle,
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                            },
                            colors = TopAppBarDefaults.topAppBarColors(
                                containerColor = MaterialTheme.colorScheme.surface,
                                scrolledContainerColor = MaterialTheme.colorScheme.surface,
                            ),
                            scrollBehavior = scrollBehavior,
                        )
                    }
                },
                bottomBar = {
                    NavigationBar {
                        pageSpecs.forEachIndexed { index, page ->
                            NavigationBarItem(
                                selected = activePageIndex == index,
                                onClick = { onPageSelected(index) },
                                icon = { Icon(page.icon, contentDescription = page.title) },
                                label = { Text(page.title) },
                            )
                        }
                    }
                },
            ) { paddingValues ->
                HorizontalPager(
                    state = pagerState,
                    modifier = Modifier.fillMaxSize(),
                    overscrollEffect = null,
                ) { page ->
                    when (page) {
                        0 -> HomePage(hookStatus = hookStatus, configState = configState, callbacks = homeCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage)
                        1 -> ConfigPage(hookStatus = hookStatus, state = configState, callbacks = configCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage)
                        2 -> DebugPage(state = debugState, callbacks = debugCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage)
                        else -> SettingsPage(state = settingsState, callbacks = settingsCallbacks, contentPadding = paddingValues, isCurrentPage = page == pagerState.currentPage)
                    }
                }
            }
        }
    }
}

private suspend fun androidx.compose.foundation.pager.PagerState.springScrollToPage(target: Int) {
    if (target !in 0 until pageCount) return
    scroll(MutatePriority.UserInput) {
        val tension = 322.2f
        val damping = 32.31f
        val pageSize = layoutInfo.pageSize + layoutInfo.pageSpacing
        val distance = target - currentPage - currentPageOffsetFraction
        val scrollPixels = distance * pageSize
        var current = 0f
        var velocity = 0f
        var lastNanos = 0L
        var finished = false
        withFrameNanos { lastNanos = it }
        while (!finished) {
            withFrameNanos { frameNanos ->
                val dt = ((frameNanos - lastNanos) / 1e9f).coerceAtMost(0.016f)
                lastNanos = frameNanos
                velocity = velocity * (1f - damping * dt) + tension * (scrollPixels - current) * dt
                val newPos = current + dt * velocity
                val delta = newPos - current
                if (abs(delta) > 0.5f) {
                    val consumed = scrollBy(delta)
                    current += consumed
                    if (abs(delta - consumed) > 0.1f) {
                        finished = true
                    }
                } else {
                    current = newPos
                }
                if (abs(velocity) < 0.1f && abs(scrollPixels - current) < 1.0f) {
                    finished = true
                }
            }
        }
    }
}

// For Android Studio preview compose interface.
@Preview(showBackground = true, device = "id:pixel_9_pro")
@Composable
private fun PreviewMainPage() {
    io.github.xiaotong6666.fusehide.ui.theme.fuseHideTheme {
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
            homeCallbacks = HomeCallbacks(
                onStatusClick = {},
            ),
            configCallbacks = ConfigCallbacks(
                onStatusClick = {},
                onEnableHideAllRootEntriesChanged = {},
                onHideAllRootEntriesExemptionsChanged = {},
                onHiddenTargetsChanged = {},
                onHiddenPackagesChanged = {},
                onApplyConfigClick = {},
                onResetConfigClick = {},
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
            settingsState = SettingsUiState(uiMode = UiMode.Miuix),
            settingsCallbacks = SettingsCallbacks(onToggleUiMode = {}),
        )
    }
}
