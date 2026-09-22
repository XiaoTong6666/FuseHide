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

@file:Suppress("ktlint:standard:function-naming", "UNUSED_PARAMETER")

package io.github.xiaotong6666.fusehide.ui.feature.config.applist

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.AppListGroupMaterial
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.AppListGroupMiuix
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.GroupedApps
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.SearchStatus
import io.github.xiaotong6666.uihelper.adaptive.WarningBanner
import io.github.xiaotong6666.uihelper.chrome.FilterableListContent
import io.github.xiaotong6666.uihelper.chrome.FilterableListHost
import io.github.xiaotong6666.uihelper.chrome.FilterableListRefreshTexts
import io.github.xiaotong6666.uihelper.chrome.SearchPageState
import io.github.xiaotong6666.uihelper.extensions.androidapp.AppIconCache
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun AppListScreen(
    state: ConfigUiState,
    callbacks: io.github.xiaotong6666.fusehide.ui.core.model.ConfigCallbacks,
    appListViewModel: AppListViewModel,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    onNavigateToGlobalConfig: () -> Unit,
    onNavigateToAppConfig: (String) -> Unit,
) {
    val uiState by appListViewModel.uiState.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val density = LocalDensity.current
    val bottomInnerPadding = contentPadding.calculateBottomPadding() + 8.dp
    var hasActivated by rememberSaveable { mutableStateOf(false) }
    val localizedSearchPlaceholder = stringResource(R.string.search_apps_placeholder)
    val enabledLabel = stringResource(R.string.app_hide_enabled_label)
    val unsavedChangesText = stringResource(R.string.unsaved_config_changes)
    val hiddenPackages = remember(state.currentHideConfig) { hiddenPackageSet(state) }
    val orderedGroups = remember(uiState.groupedApps, hiddenPackages) {
        prioritizeHiddenGroups(uiState.groupedApps, hiddenPackages)
    }
    val orderedSearchResults = remember(uiState.searchResults, hiddenPackages) {
        prioritizeHiddenGroups(uiState.searchResults, hiddenPackages)
    }
    val appIconSizePx = remember(density) {
        with(density) { 48.dp.roundToPx() }
    }

    LaunchedEffect(isCurrentPage) {
        if (isCurrentPage) hasActivated = true
    }

    LaunchedEffect(hasActivated, appListViewModel) {
        if (hasActivated) {
            appListViewModel.loadAppList().join()
        }
    }

    LaunchedEffect(orderedGroups, appIconSizePx) {
        AppIconCache.preloadIcons(
            context = context.applicationContext,
            applicationInfos = orderedGroups.asSequence().map { it.primary.applicationInfo }.asIterable(),
            sizePx = appIconSizePx,
        )
    }

    LaunchedEffect(localizedSearchPlaceholder, uiState.searchStatus.placeholder) {
        if (uiState.searchStatus.placeholder != localizedSearchPlaceholder) {
            appListViewModel.updateSearchStatus(
                uiState.searchStatus.copy(placeholder = localizedSearchPlaceholder),
            )
        }
    }

    FilterableListHost(
        state = uiState.searchStatus,
        onStateChange = appListViewModel::updateSearchStatus,
        isRefreshing = uiState.isRefreshing,
        onRefresh = { appListViewModel.loadAppList(force = true) },
        contentPadding = contentPadding,
        isCurrentPage = isCurrentPage,
        refreshTexts = FilterableListRefreshTexts(
            pulling = stringResource(R.string.refresh_pulling),
            release = stringResource(R.string.refresh_release),
            refreshing = stringResource(R.string.refresh_refreshing),
            complete = stringResource(R.string.refresh_complete),
        ),
        content = FilterableListContent(
            materialMain = { contentModifier ->
                val expandedUids = remember { mutableStateOf(setOf<Int>()) }
                LazyColumn(
                    state = rememberLazyListState(),
                    modifier = contentModifier,
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(2.dp),
                    contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 0.dp, bottom = 16.dp + bottomInnerPadding),
                ) {
                    item(key = "unsaved-warning", contentType = "warning") {
                        AnimatedVisibility(
                            visible = state.draftVsAppliedDiff.hasDifferences,
                            enter = fadeIn() + expandVertically(),
                            exit = shrinkVertically() + fadeOut(),
                        ) {
                            WarningBanner(
                                message = unsavedChangesText,
                                modifier = Modifier.padding(bottom = 10.dp),
                                onClick = onNavigateToGlobalConfig,
                            )
                        }
                    }
                    itemsIndexed(
                        orderedGroups,
                        key = { _, item -> item.uid },
                        contentType = { _, _ -> "group" },
                    ) { index, group ->
                        val expanded = expandedUids.value.contains(group.uid)
                        AppListGroupMaterial(
                            group = group,
                            hiddenPackages = hiddenPackages,
                            enabledLabel = enabledLabel,
                            expanded = expanded,
                            onToggleExpand = {
                                if (group.apps.size > 1) {
                                    expandedUids.value = if (expanded) expandedUids.value - group.uid else expandedUids.value + group.uid
                                }
                            },
                            onOpenApp = onNavigateToAppConfig,
                            index = index,
                            count = orderedGroups.size,
                        )
                    }
                }
            },
            materialSearchResults = { contentModifier, closeSearch ->
                val searchListState = rememberLazyListState()
                LaunchedEffect(uiState.searchStatus.query) {
                    searchListState.requestScrollToItem(0)
                }
                LazyColumn(
                    state = searchListState,
                    modifier = contentModifier,
                    verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(2.dp),
                    contentPadding = PaddingValues(start = 16.dp, end = 16.dp, bottom = 32.dp),
                ) {
                    itemsIndexed(
                        orderedSearchResults,
                        key = { _, item -> item.uid },
                        contentType = { _, _ -> "group" },
                    ) { index, group ->
                        AppListGroupMaterial(
                            group = group,
                            hiddenPackages = hiddenPackages,
                            enabledLabel = enabledLabel,
                            expanded = group.apps.size > 1,
                            onToggleExpand = {},
                            onOpenApp = {
                                closeSearch()
                                onNavigateToAppConfig(it)
                            },
                            matchedPackageNames = group.matchedPackageNames,
                            alwaysShowChildren = true,
                            index = index,
                            count = orderedSearchResults.size,
                        )
                    }
                }
            },
            miuixMain = { contentModifier ->
                val layoutDirection = LocalLayoutDirection.current
                val expandedUids = remember { mutableStateOf(setOf<Int>()) }
                val listState = rememberLazyListState()

                LazyColumn(
                    state = listState,
                    modifier = Modifier
                        .fillMaxHeight()
                        .scrollEndHaptic()
                        .overScrollVertical()
                        .then(contentModifier),
                    contentPadding = PaddingValues(
                        start = contentPadding.calculateStartPadding(layoutDirection),
                        end = contentPadding.calculateEndPadding(layoutDirection),
                    ),
                    overscrollEffect = null,
                ) {
                    item(key = "unsaved-warning", contentType = "warning") {
                        AnimatedVisibility(
                            visible = state.draftVsAppliedDiff.hasDifferences,
                            enter = fadeIn() + expandVertically(),
                            exit = shrinkVertically() + fadeOut(),
                        ) {
                            WarningBanner(
                                message = unsavedChangesText,
                                modifier = Modifier
                                    .padding(horizontal = 12.dp)
                                    .padding(bottom = 12.dp),
                                onClick = onNavigateToGlobalConfig,
                            )
                        }
                    }
                    itemsIndexed(orderedGroups, key = { _, item -> item.uid }, contentType = { _, _ -> "group" }) { _, group ->
                        val expanded = expandedUids.value.contains(group.uid)
                        AppListGroupMiuix(
                            group = group,
                            hiddenPackages = hiddenPackages,
                            enabledLabel = enabledLabel,
                            expanded = expanded,
                            onToggleExpand = {
                                if (group.apps.size > 1) {
                                    expandedUids.value = if (expanded) expandedUids.value - group.uid else expandedUids.value + group.uid
                                }
                            },
                            onOpenApp = onNavigateToAppConfig,
                        )
                    }
                    item { Spacer(Modifier.height(bottomInnerPadding)) }
                }
            },
            miuixSearchResults = { contentModifier, closeSearch ->
                val expandedSearchUids = remember { mutableStateOf(setOf<Int>()) }
                val searchListState = rememberLazyListState()
                LaunchedEffect(orderedSearchResults) {
                    expandedSearchUids.value = orderedSearchResults.filter { it.apps.size > 1 }.map { it.uid }.toSet()
                }
                LaunchedEffect(uiState.searchStatus.query) {
                    searchListState.requestScrollToItem(0)
                }
                LazyColumn(
                    state = searchListState,
                    modifier = Modifier
                        .overScrollVertical()
                        .then(contentModifier),
                ) {
                    item { Spacer(Modifier.height(6.dp)) }
                    itemsIndexed(orderedSearchResults, key = { _, item -> item.uid }, contentType = { _, _ -> "group" }) { _, group ->
                        val expanded = expandedSearchUids.value.contains(group.uid)
                        AppListGroupMiuix(
                            group = group,
                            hiddenPackages = hiddenPackages,
                            enabledLabel = enabledLabel,
                            expanded = expanded,
                            onToggleExpand = {
                                if (group.apps.size > 1) {
                                    expandedSearchUids.value = if (expanded) expandedSearchUids.value - group.uid else expandedSearchUids.value + group.uid
                                }
                            },
                            onOpenApp = {
                                closeSearch()
                                onNavigateToAppConfig(it)
                            },
                            matchedPackageNames = group.matchedPackageNames,
                            alwaysShowChildren = true,
                        )
                    }
                    item { Spacer(Modifier.height(bottomInnerPadding)) }
                }
            },
        ),
    )
}

private fun hiddenPackageSet(globalState: ConfigUiState): Set<String> = buildSet {
    addAll(globalState.currentHideConfig.hiddenPackages)
    globalState.currentHideConfig.packageRules.forEach { add(it.packageName) }
}

private fun prioritizeHiddenGroups(groups: List<GroupedApps>, hiddenPackages: Set<String>): List<GroupedApps> {
    val (hidden, normal) = groups.partition { it.primary.packageName in hiddenPackages }
    return hidden + normal
}
