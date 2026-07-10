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

package io.github.xiaotong6666.uihelper.chrome

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.spring
import androidx.compose.foundation.MutatePriority
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.union
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExperimentalMaterial3ExpressiveApi
import androidx.compose.material3.Icon
import androidx.compose.material3.LargeFlexibleTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ShortNavigationBar
import androidx.compose.material3.ShortNavigationBarItem
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.nestedscroll.nestedScroll
import io.github.xiaotong6666.uihelper.material.scaffold.ExpressiveScaffold
import io.github.xiaotong6666.uihelper.material.scaffold.expressiveTopAppBarColors
import io.github.xiaotong6666.uihelper.material.scaffold.materialScaffoldEdgeToEdgeInsets
import io.github.xiaotong6666.uihelper.material.scaffold.materialTopBarEdgeToEdgeInsets
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.job
import kotlinx.coroutines.launch
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import kotlin.math.abs
import top.yukonga.miuix.kmp.basic.NavigationBar as MiuixNavigationBar
import top.yukonga.miuix.kmp.basic.NavigationBarItem as MiuixNavigationBarItem

data class NavigationShellItem(
    val title: String,
    val icon: ImageVector,
    val selectedIcon: ImageVector = icon,
)

private class NavigationShellPagerState(
    val pagerState: PagerState,
    private val coroutineScope: CoroutineScope,
) {
    var selectedPage by mutableIntStateOf(pagerState.currentPage)
        private set

    var isNavigating by mutableStateOf(false)
        private set

    private var navJob: Job? = null

    fun animateToPage(targetIndex: Int) {
        if (targetIndex == selectedPage) return

        navJob?.cancel()
        selectedPage = targetIndex
        isNavigating = true

        navJob = coroutineScope.launch {
            val myJob = coroutineContext.job
            try {
                pagerState.springAnimateToPage(targetIndex)
            } finally {
                if (navJob == myJob) {
                    isNavigating = false
                    if (pagerState.settledPage != targetIndex) {
                        selectedPage = pagerState.settledPage
                    }
                }
            }
        }
    }

    fun syncPage() {
        if (!isNavigating && selectedPage != pagerState.currentPage) {
            selectedPage = pagerState.currentPage
        }
    }

    suspend fun cancelNavigation() {
        navJob?.cancelAndJoin()
        navJob = null
        isNavigating = false
        selectedPage = pagerState.settledPage
    }
}

@Composable
private fun rememberNavigationShellPagerState(
    pagerState: PagerState,
    coroutineScope: CoroutineScope = rememberCoroutineScope(),
): NavigationShellPagerState = remember(pagerState, coroutineScope) {
    NavigationShellPagerState(pagerState, coroutineScope)
}

@OptIn(ExperimentalMaterial3Api::class, ExperimentalMaterial3ExpressiveApi::class)
@Composable
fun AdaptiveNavigationShell(
    items: List<NavigationShellItem>,
    selectedIndex: Int,
    onSelectedIndexChange: (Int) -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable (pageIndex: Int, contentPadding: PaddingValues, isCurrentPage: Boolean, pageModifier: Modifier) -> Unit,
) {
    if (items.isEmpty()) return

    val coercedSelectedIndex = selectedIndex.coerceIn(0, items.lastIndex)
    val pagerState = rememberPagerState(initialPage = coercedSelectedIndex, pageCount = { items.size })
    val shellPagerState = rememberNavigationShellPagerState(pagerState)
    val settledPage = pagerState.settledPage
    val activePageIndex = shellPagerState.selectedPage.coerceIn(0, items.lastIndex)
    val activeItem = items[activePageIndex]
    val appChromeState = rememberAppChromeState()
    val chromeSpec = appChromeState.spec
    val miuixScrollBehavior = MiuixScrollBehavior()
    val onPageSelected: (Int) -> Unit = { index ->
        if (shellPagerState.selectedPage != index) {
            shellPagerState.animateToPage(index)
        }
    }

    LaunchedEffect(pagerState.currentPage) {
        shellPagerState.syncPage()
    }

    LaunchedEffect(settledPage) {
        if (selectedIndex != settledPage) {
            onSelectedIndexChange(settledPage)
        }
    }

    LaunchedEffect(selectedIndex) {
        val targetIndex = selectedIndex.coerceIn(0, items.lastIndex)
        if (!shellPagerState.isNavigating && targetIndex != shellPagerState.selectedPage) {
            shellPagerState.animateToPage(targetIndex)
        }
    }

    LaunchedEffect(pagerState.isScrollInProgress) {
        if (!pagerState.isScrollInProgress && shellPagerState.isNavigating) {
            shellPagerState.cancelNavigation()
        }
    }

    when (LocalUiMode.current) {
        UiMode.Miuix -> {
            val pageHostHandle = remember(miuixScrollBehavior) {
                PageHostHandle(
                    nestedScrollConnection = miuixScrollBehavior.nestedScrollConnection,
                    collapsedFractionProvider = { miuixScrollBehavior.state.collapsedFraction },
                )
            }
            top.yukonga.miuix.kmp.basic.Scaffold(
                modifier = Modifier
                    .fillMaxSize()
                    .then(modifier),
                topBar = {
                    val defaultTopBar: ComposableContent = {
                        top.yukonga.miuix.kmp.basic.TopAppBar(
                            title = activeItem.title,
                            color = MiuixTheme.colorScheme.surface,
                            titleColor = MiuixTheme.colorScheme.onSurface,
                            actions = {
                                chromeSpec.miuixActions?.invoke()
                            },
                            scrollBehavior = miuixScrollBehavior,
                        )
                    }
                    val miuixTopBar = chromeSpec.miuixTopBar
                    val miuixTopBarWrapper = chromeSpec.miuixTopBarWrapper
                    when {
                        miuixTopBar != null -> miuixTopBar()
                        miuixTopBarWrapper != null -> miuixTopBarWrapper(defaultTopBar)
                        else -> defaultTopBar()
                    }
                },
                popupHost = chromeSpec.miuixPopupHost ?: {},
                bottomBar = if (chromeSpec.hideBottomBar) {
                    {}
                } else {
                    {
                        MiuixNavigationBar {
                            items.forEachIndexed { index, item ->
                                MiuixNavigationBarItem(
                                    selected = activePageIndex == index,
                                    onClick = { onPageSelected(index) },
                                    icon = item.icon,
                                    label = item.title,
                                )
                            }
                        }
                    }
                },
            ) { paddingValues ->
                CompositionLocalProvider(
                    LocalAppChromeState provides appChromeState,
                    LocalPageHostHandle provides pageHostHandle,
                    LocalMiuixNestedScrollConnection provides miuixScrollBehavior.nestedScrollConnection,
                    LocalMiuixCollapsedFractionProvider provides { miuixScrollBehavior.state.collapsedFraction },
                ) {
                    Box(modifier = Modifier.fillMaxSize()) {
                        HorizontalPager(
                            state = pagerState,
                            modifier = Modifier.fillMaxSize(),
                            overscrollEffect = null,
                        ) { page ->
                            val pageModifier = Modifier
                                .then(if (page == pagerState.currentPage) Modifier.overScrollVertical() else Modifier)
                                .then(
                                    if (chromeSpec.consumeOuterScroll) {
                                        Modifier
                                    } else {
                                        Modifier.nestedScroll(miuixScrollBehavior.nestedScrollConnection)
                                    },
                                )

                            content(
                                page,
                                paddingValues,
                                page == pagerState.currentPage,
                                pageModifier,
                            )
                        }
                        chromeSpec.overlayContent?.invoke(paddingValues)
                    }
                }
            }
        }

        UiMode.Material -> {
            val scrollBehavior = TopAppBarDefaults.exitUntilCollapsedScrollBehavior()
            val pageHostHandle = remember(scrollBehavior) {
                PageHostHandle(
                    nestedScrollConnection = scrollBehavior.nestedScrollConnection,
                    collapsedFractionProvider = { scrollBehavior.state.collapsedFraction },
                )
            }
            ExpressiveScaffold(
                modifier = Modifier
                    .fillMaxSize()
                    .then(modifier)
                    .then(
                        if (chromeSpec.consumeOuterScroll) {
                            Modifier
                        } else {
                            Modifier.nestedScroll(scrollBehavior.nestedScrollConnection)
                        },
                    ),
                contentWindowInsets = materialScaffoldEdgeToEdgeInsets(),
                topBar = chromeSpec.materialTopBar ?: {
                    LargeFlexibleTopAppBar(
                        title = { Text(text = activeItem.title) },
                        actions = {
                            chromeSpec.materialActions?.invoke(this)
                        },
                        colors = expressiveTopAppBarColors(),
                        scrollBehavior = scrollBehavior,
                        windowInsets = materialTopBarEdgeToEdgeInsets(),
                    )
                },
                bottomBar = if (chromeSpec.hideBottomBar) {
                    {}
                } else {
                    {
                        ShortNavigationBar(
                            containerColor = MaterialTheme.colorScheme.surfaceContainer,
                            windowInsets = WindowInsets.systemBars.union(WindowInsets.displayCutout).only(
                                WindowInsetsSides.Horizontal + WindowInsetsSides.Bottom,
                            ),
                        ) {
                            items.forEachIndexed { index, item ->
                                val selected = activePageIndex == index
                                ShortNavigationBarItem(
                                    selected = selected,
                                    onClick = { onPageSelected(index) },
                                    icon = {
                                        Icon(
                                            imageVector = if (selected) item.selectedIcon else item.icon,
                                            contentDescription = item.title,
                                        )
                                    },
                                    label = { Text(item.title) },
                                )
                            }
                        }
                    }
                },
            ) { paddingValues ->
                CompositionLocalProvider(
                    LocalAppChromeState provides appChromeState,
                    LocalPageHostHandle provides pageHostHandle,
                    LocalMaterialNestedScrollConnection provides scrollBehavior.nestedScrollConnection,
                ) {
                    Box(modifier = Modifier.fillMaxSize()) {
                        HorizontalPager(
                            state = pagerState,
                            modifier = Modifier.fillMaxSize(),
                            overscrollEffect = null,
                        ) { page ->
                            content(
                                page,
                                paddingValues,
                                page == pagerState.currentPage,
                                Modifier,
                            )
                        }
                        chromeSpec.overlayContent?.invoke(paddingValues)
                    }
                }
            }
        }
    }
}

private suspend fun PagerState.springAnimateToPage(target: Int) {
    if (target !in 0 until pageCount) return
    var shouldSnapToTarget = false
    scroll(MutatePriority.UserInput) {
        val pageSize = layoutInfo.pageSize + layoutInfo.pageSpacing
        val distance = target - currentPage - currentPageOffsetFraction
        val scrollPixels = distance * pageSize
        if (abs(scrollPixels) <= 0.5f) {
            return@scroll
        }

        var consumedScroll = 0f
        var skipScroll = false
        Animatable(0f).animateTo(
            targetValue = scrollPixels,
            animationSpec = spring(
                stiffness = 322.2f,
                dampingRatio = 32.31f / (2f * kotlin.math.sqrt(322.2f)),
                visibilityThreshold = 0.5f,
            ),
        ) {
            if (skipScroll) return@animateTo

            val delta = value - consumedScroll
            if (abs(delta) > 0.5f) {
                val consumed = scrollBy(delta)
                consumedScroll += consumed
                if (abs(delta - consumed) > 0.1f) {
                    shouldSnapToTarget = true
                    skipScroll = true
                }
            } else {
                consumedScroll = value
            }

            if (abs(velocity) < 0.1f && abs(scrollPixels - consumedScroll) < 1.0f) {
                skipScroll = true
            }
        }

        val remaining = scrollPixels - consumedScroll
        if (abs(remaining) > 0.5f) {
            scrollBy(remaining)
        }
    }

    if (shouldSnapToTarget || currentPage != target) {
        scrollToPage(target)
    }
}
