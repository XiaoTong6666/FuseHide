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

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.text.input.rememberTextFieldState
import androidx.compose.foundation.text.input.setTextAndPlaceCursorAtEnd
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.ExpandedFullScreenContainedSearchBar
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButtonDefaults
import androidx.compose.material3.SearchBar
import androidx.compose.material3.SearchBarDefaults
import androidx.compose.material3.SearchBarValue
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.material3.pulltorefresh.PullToRefreshDefaults
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.material3.rememberContainedSearchBarState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInParent
import androidx.compose.ui.layout.positionInWindow
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.uihelper.miuix.primitive.CollapsedSearchBox
import io.github.xiaotong6666.uihelper.miuix.primitive.SearchBarFake
import io.github.xiaotong6666.uihelper.miuix.primitive.SearchOverlayPager
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import top.yukonga.miuix.kmp.basic.PullToRefresh
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState as rememberMaterialPullToRefreshState
import top.yukonga.miuix.kmp.basic.rememberPullToRefreshState as rememberMiuixPullToRefreshState

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FilterableListHost(
    state: SearchPageState,
    onStateChange: (SearchPageState) -> Unit,
    isRefreshing: Boolean,
    onRefresh: () -> Unit,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    materialActions: (@Composable androidx.compose.foundation.layout.RowScope.() -> Unit)? = null,
    miuixActions: (@Composable () -> Unit)? = null,
    materialMainContent: @Composable (contentModifier: Modifier, searchBar: @Composable () -> Unit) -> Unit,
    materialSearchResultContent: @Composable (contentModifier: Modifier, closeSearch: () -> Unit) -> Unit,
    miuixMainContent: @Composable (contentModifier: Modifier, listTopPadding: Dp, dynamicTopPadding: Dp) -> Unit,
    miuixSearchResultContent: @Composable (contentModifier: Modifier, dynamicTopPadding: Dp, closeSearch: () -> Unit) -> Unit,
    miuixDefaultResultContent: (@Composable () -> Unit)? = null,
    miuixRefreshTexts: List<String>? = null,
    miuixCollapsedSearchField: @Composable (dynamicTopPadding: Dp, placeholder: String) -> Unit = { dynamicTopPadding, placeholder ->
        SearchBarFake(
            label = placeholder,
            searchBarTopPadding = dynamicTopPadding,
            bottomPadding = 0.dp,
        )
    },
) {
    when (io.github.xiaotong6666.uihelper.mode.LocalUiMode.current) {
        io.github.xiaotong6666.uihelper.mode.UiMode.Material -> MaterialFilterableListHost(
            state = state,
            onStateChange = onStateChange,
            isRefreshing = isRefreshing,
            onRefresh = onRefresh,
            contentPadding = contentPadding,
            isCurrentPage = isCurrentPage,
            materialActions = materialActions,
            materialMainContent = materialMainContent,
            materialSearchResultContent = materialSearchResultContent,
        )

        io.github.xiaotong6666.uihelper.mode.UiMode.Miuix -> MiuixFilterableListHost(
            state = state,
            onStateChange = onStateChange,
            isRefreshing = isRefreshing,
            onRefresh = onRefresh,
            contentPadding = contentPadding,
            isCurrentPage = isCurrentPage,
            miuixActions = miuixActions,
            miuixMainContent = miuixMainContent,
            miuixSearchResultContent = miuixSearchResultContent,
            miuixDefaultResultContent = miuixDefaultResultContent,
            miuixRefreshTexts = miuixRefreshTexts,
            miuixCollapsedSearchField = miuixCollapsedSearchField,
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun MaterialFilterableListHost(
    state: SearchPageState,
    onStateChange: (SearchPageState) -> Unit,
    isRefreshing: Boolean,
    onRefresh: () -> Unit,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean,
    materialActions: (@Composable androidx.compose.foundation.layout.RowScope.() -> Unit)? = null,
    materialMainContent: @Composable (contentModifier: Modifier, searchBar: @Composable () -> Unit) -> Unit,
    materialSearchResultContent: @Composable (contentModifier: Modifier, closeSearch: () -> Unit) -> Unit,
) {
    val pullToRefreshState = rememberMaterialPullToRefreshState()
    val keyboardController = LocalSoftwareKeyboardController.current
    val focusManager = LocalFocusManager.current
    val scaledDensity = LocalDensity.current
    val interactionSource = remember { MutableInteractionSource() }
    val scope = rememberCoroutineScope()
    val searchBarState = rememberContainedSearchBarState()
    val textFieldState = rememberTextFieldState()
    val currentQuery = textFieldState.text.toString()
    val latestQuery by rememberUpdatedState(state.query)
    val isSearchExpanded = searchBarState.currentValue != SearchBarValue.Collapsed || searchBarState.targetValue != SearchBarValue.Collapsed
    var previousSearchBarValue by remember { mutableStateOf(searchBarState.currentValue) }
    var shouldClearOnCollapse by remember { mutableStateOf(true) }

    val clearSearchText: () -> Unit = {
        textFieldState.setTextAndPlaceCursorAtEnd("")
        onStateChange(state.copy(query = ""))
    }
    val collapseAndClear: () -> Unit = {
        shouldClearOnCollapse = false
        clearSearchText()
        scope.launch { searchBarState.animateToCollapsed() }
        focusManager.clearFocus()
        keyboardController?.hide()
    }

    DisposableEffect(Unit) {
        onDispose {
            keyboardController?.hide()
        }
    }

    LaunchedEffect(state.query) {
        val current = textFieldState.text.toString()
        if (current != state.query) {
            textFieldState.setTextAndPlaceCursorAtEnd(state.query)
        }
    }

    LaunchedEffect(textFieldState, latestQuery) {
        snapshotFlow { textFieldState.text.toString() }
            .distinctUntilChanged()
            .collect { value ->
                if (value != latestQuery) {
                    onStateChange(state.copy(query = value))
                }
            }
    }

    LaunchedEffect(searchBarState) {
        snapshotFlow { searchBarState.currentValue }
            .distinctUntilChanged()
            .collect { value ->
                val collapsedFromExpanded =
                    previousSearchBarValue != SearchBarValue.Collapsed && value == SearchBarValue.Collapsed
                previousSearchBarValue = value
                if (collapsedFromExpanded) {
                    if (shouldClearOnCollapse) {
                        clearSearchText()
                    }
                    shouldClearOnCollapse = true
                    focusManager.clearFocus()
                    keyboardController?.hide()
                }
            }
    }

    BackHandler(isSearchExpanded) {
        if (isSearchExpanded) {
            collapseAndClear()
        }
    }

    val inputField: @Composable () -> Unit = {
        CompositionLocalProvider(LocalDensity provides scaledDensity) {
            SearchBarDefaults.InputField(
                textFieldState = textFieldState,
                searchBarState = searchBarState,
                onSearch = {
                    focusManager.clearFocus()
                    keyboardController?.hide()
                },
                placeholder = { androidx.compose.material3.Text(state.placeholder) },
                keyboardOptions = androidx.compose.foundation.text.KeyboardOptions(imeAction = ImeAction.Search),
                leadingIcon = {
                    if (isSearchExpanded) {
                        androidx.compose.material3.IconButton(
                            modifier = Modifier.padding(end = 8.dp),
                            onClick = { collapseAndClear() },
                            colors = IconButtonDefaults.iconButtonColors(
                                containerColor = androidx.compose.material3.MaterialTheme.colorScheme.surfaceContainerHighest,
                                contentColor = androidx.compose.material3.MaterialTheme.colorScheme.onSurfaceVariant,
                            ),
                        ) {
                            Icon(Icons.AutoMirrored.Rounded.ArrowBack, null)
                        }
                    } else {
                        Icon(Icons.Filled.Search, null)
                    }
                },
                trailingIcon = {
                    if (isSearchExpanded && currentQuery.isNotEmpty()) {
                        androidx.compose.material3.IconButton(onClick = { clearSearchText() }) {
                            Icon(Icons.Filled.Close, null)
                        }
                    }
                },
                interactionSource = interactionSource,
            )
        }
    }

    PageHost(
        spec = { pageHost ->
            AppChromeSpec(
                materialActions = materialActions,
                overlayContent = {
                    ExpandedFullScreenContainedSearchBar(
                        state = searchBarState,
                        inputField = inputField,
                        windowInsets = { SearchBarDefaults.fullScreenWindowInsets.only(WindowInsetsSides.Top + WindowInsetsSides.Horizontal) },
                    ) {
                        materialSearchResultContent(
                            pageHost.nestedScroll(Modifier.fillMaxSize()),
                            collapseAndClear,
                        )
                    }
                },
            )
        },
        enabled = isCurrentPage,
    ) { pageHost ->
        PullToRefreshBox(
            modifier = Modifier.fillMaxSize().padding(top = contentPadding.calculateTopPadding()),
            isRefreshing = isRefreshing,
            onRefresh = onRefresh,
            state = pullToRefreshState,
            indicator = {
                PullToRefreshDefaults.LoadingIndicator(
                    modifier = Modifier
                        .align(Alignment.TopCenter)
                        .padding(top = 74.dp),
                    isRefreshing = isRefreshing,
                    state = pullToRefreshState,
                )
            },
        ) {
            materialMainContent(
                pageHost.nestedScroll(Modifier.fillMaxSize()),
            ) {
                SearchBar(
                    modifier = Modifier
                        .fillMaxWidth()
                        .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Horizontal))
                        .padding(bottom = 18.dp),
                    state = searchBarState,
                    inputField = inputField,
                    colors = SearchBarDefaults.colors(containerColor = androidx.compose.material3.MaterialTheme.colorScheme.surfaceContainerHighest),
                )
            }
        }
    }
}

@Composable
private fun MiuixFilterableListHost(
    state: SearchPageState,
    onStateChange: (SearchPageState) -> Unit,
    isRefreshing: Boolean,
    onRefresh: () -> Unit,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean,
    miuixActions: (@Composable () -> Unit)? = null,
    miuixMainContent: @Composable (contentModifier: Modifier, listTopPadding: Dp, dynamicTopPadding: Dp) -> Unit,
    miuixSearchResultContent: @Composable (contentModifier: Modifier, dynamicTopPadding: Dp, closeSearch: () -> Unit) -> Unit,
    miuixDefaultResultContent: (@Composable () -> Unit)? = null,
    miuixRefreshTexts: List<String>? = null,
    miuixCollapsedSearchField: @Composable (dynamicTopPadding: Dp, placeholder: String) -> Unit,
) {
    val density = LocalDensity.current
    val layoutDirection = LocalLayoutDirection.current
    val refreshTexts = miuixRefreshTexts ?: listOf(
        "Pull to refresh",
        "Release to refresh",
        "Refreshing",
        "Refresh complete",
    )

    PageHost(
        spec = { pageHost ->
            AppChromeSpec(
                miuixTopBarWrapper = { content ->
                    state.TopAppBarAnim {
                        content()
                    }
                },
                miuixActions = miuixActions,
                miuixPopupHost = {
                    state.SearchOverlayPager(
                        onStateChange = onStateChange,
                        defaultResult = { miuixDefaultResultContent?.invoke() },
                        searchBarTopPadding = 12.dp * (1f - pageHost.collapsedFraction),
                    ) {
                        miuixSearchResultContent(
                            Modifier.fillMaxSize(),
                            12.dp * (1f - pageHost.collapsedFraction),
                        ) {
                            onStateChange(state.copy(query = "", current = SearchPageState.Status.COLLAPSED))
                        }
                    }
                },
                consumeOuterScroll = true,
            )
        },
        enabled = isCurrentPage,
    ) { pageHost ->
        val dynamicTopPadding = 12.dp * (1f - pageHost.collapsedFraction)

        state.CollapsedSearchBox {
            val pullToRefreshState = rememberMiuixPullToRefreshState()
            var searchContainerBottom by remember { mutableStateOf(0.dp) }
            val searchTopPadding = contentPadding.calculateTopPadding()
            val listSpacingBelowSearch = 12.dp

            Box(modifier = Modifier.fillMaxSize()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = searchTopPadding)
                        .alpha(if (state.isCollapsed()) 1f else 0f)
                        .onGloballyPositioned { coordinates ->
                            with(density) {
                                val newOffsetY = coordinates.positionInWindow().y.toDp()
                                if (state.anchorOffsetY != newOffsetY) {
                                    onStateChange(state.copy(anchorOffsetY = newOffsetY))
                                }
                                searchContainerBottom = coordinates.positionInParent().y.toDp() + coordinates.size.height.toDp() + listSpacingBelowSearch
                            }
                        }
                        .then(
                            if (state.isCollapsed()) {
                                Modifier.pointerInput(state) {
                                    detectTapGestures {
                                        onStateChange(state.copy(current = SearchPageState.Status.EXPANDING))
                                    }
                                }
                            } else {
                                Modifier
                            },
                        ),
                ) {
                    miuixCollapsedSearchField(dynamicTopPadding, state.placeholder)
                }

                PullToRefresh(
                    modifier = Modifier.padding(top = searchContainerBottom),
                    isRefreshing = isRefreshing,
                    pullToRefreshState = pullToRefreshState,
                    onRefresh = onRefresh,
                    refreshTexts = refreshTexts,
                    contentPadding = PaddingValues(
                        start = contentPadding.calculateStartPadding(layoutDirection),
                        end = contentPadding.calculateEndPadding(layoutDirection),
                    ),
                ) {
                    miuixMainContent(
                        pageHost.nestedScroll(Modifier),
                        searchContainerBottom,
                        dynamicTopPadding,
                    )
                }
            }
        }
    }
}
