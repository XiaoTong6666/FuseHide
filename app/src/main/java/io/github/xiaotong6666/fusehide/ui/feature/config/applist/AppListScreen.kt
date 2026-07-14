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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.AppListGroupMaterial
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.AppListGroupMiuix
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.AppListSearchFieldMiuix
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.GroupedApps
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.SearchStatus
import io.github.xiaotong6666.uihelper.adaptive.WarningBanner
import io.github.xiaotong6666.uihelper.chrome.FilterableListHost
import io.github.xiaotong6666.uihelper.chrome.SearchPageState
import io.github.xiaotong6666.uihelper.material.materialChromeIconButtonColors
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme
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
    val uiState by appListViewModel.uiState.collectAsState()
    val bottomInnerPadding = contentPadding.calculateBottomPadding() + 8.dp
    var hasActivated by remember { mutableStateOf(false) }
    val hiddenPackages = remember(state.currentHideConfig) { hiddenPackageSet(state) }
    val orderedGroups = remember(uiState.groupedApps, hiddenPackages) {
        prioritizeHiddenGroups(uiState.groupedApps, hiddenPackages)
    }
    val orderedSearchResults = remember(uiState.searchResults, hiddenPackages) {
        prioritizeHiddenGroups(uiState.searchResults, hiddenPackages)
    }

    if (isCurrentPage) {
        hasActivated = true
    }

    if (hasActivated) {
        LaunchedEffect(appListViewModel) {
            appListViewModel.loadAppList().join()
        }
    }

    FilterableListHost(
        state = uiState.searchStatus,
        onStateChange = appListViewModel::updateSearchStatus,
        isRefreshing = uiState.isRefreshing,
        onRefresh = { appListViewModel.loadAppList(force = true) },
        contentPadding = contentPadding,
        isCurrentPage = isCurrentPage,
        materialActions = {
            androidx.compose.material3.IconButton(
                onClick = onNavigateToGlobalConfig,
                colors = materialChromeIconButtonColors(),
            ) {
                androidx.compose.material3.Icon(
                    imageVector = Icons.Default.Settings,
                    contentDescription = stringResource(R.string.global_hide_config_title),
                )
            }
        },
        miuixActions = {
            IconButton(onClick = onNavigateToGlobalConfig) {
                androidx.compose.material3.Icon(
                    imageVector = Icons.Default.Settings,
                    tint = colorScheme.onSurface,
                    contentDescription = stringResource(R.string.global_hide_config_title),
                )
            }
        },
        materialMainContent = { contentModifier, searchBar ->
            val expandedUids = remember { mutableStateOf(setOf<Int>()) }
            LazyColumn(
                state = rememberLazyListState(),
                modifier = contentModifier,
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(2.dp),
                contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 0.dp, bottom = 16.dp + bottomInnerPadding),
            ) {
                item { searchBar() }
                if (state.draftVsAppliedDiff.hasDifferences) {
                    item {
                        WarningBanner(
                            message = stringResource(R.string.unsaved_config_changes),
                            modifier = Modifier.padding(bottom = 10.dp),
                            onClick = onNavigateToGlobalConfig,
                        )
                    }
                }
                itemsIndexed(orderedGroups, key = { _, item -> item.uid }) { index, group ->
                    val expanded = expandedUids.value.contains(group.uid)
                    AppListGroupMaterial(
                        group = group,
                        hiddenPackages = hiddenPackages,
                        enabledLabel = stringResource(R.string.app_hide_enabled_label),
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
        materialSearchResultContent = { contentModifier, closeSearch ->
            LazyColumn(
                state = rememberLazyListState(),
                modifier = contentModifier,
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(2.dp),
                contentPadding = PaddingValues(start = 16.dp, end = 16.dp, bottom = 32.dp),
            ) {
                itemsIndexed(orderedSearchResults, key = { _, item -> item.uid }) { index, group ->
                    AppListGroupMaterial(
                        group = group,
                        hiddenPackages = hiddenPackages,
                        enabledLabel = stringResource(R.string.app_hide_enabled_label),
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
        miuixMainContent = { contentModifier, _, _ ->
            val layoutDirection = LocalLayoutDirection.current
            val expandedUids = remember { mutableStateOf(setOf<Int>()) }
            val listState = rememberLazyListState()

            LazyColumn(
                state = listState,
                modifier = contentModifier
                    .fillMaxHeight()
                    .scrollEndHaptic()
                    .overScrollVertical(),
                contentPadding = PaddingValues(
                    start = contentPadding.calculateStartPadding(layoutDirection),
                    end = contentPadding.calculateEndPadding(layoutDirection),
                ),
                overscrollEffect = null,
            ) {
                if (state.draftVsAppliedDiff.hasDifferences) {
                    item {
                        WarningBanner(
                            message = stringResource(R.string.unsaved_config_changes),
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
                        enabledLabel = stringResource(R.string.app_hide_enabled_label),
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
        miuixSearchResultContent = { contentModifier, _, closeSearch ->
            val expandedSearchUids = remember { mutableStateOf(setOf<Int>()) }
            LaunchedEffect(orderedSearchResults) {
                expandedSearchUids.value = orderedSearchResults.filter { it.apps.size > 1 }.map { it.uid }.toSet()
            }
            LazyColumn(modifier = contentModifier.overScrollVertical()) {
                item { Spacer(Modifier.height(6.dp)) }
                itemsIndexed(orderedSearchResults, key = { _, item -> item.uid }, contentType = { _, _ -> "group" }) { _, group ->
                    val expanded = expandedSearchUids.value.contains(group.uid)
                    AppListGroupMiuix(
                        group = group,
                        hiddenPackages = hiddenPackages,
                        enabledLabel = stringResource(R.string.app_hide_enabled_label),
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
        miuixDefaultResultContent = {
            Box(modifier = Modifier.fillMaxSize())
        },
        miuixRefreshTexts = listOf(
            stringResource(R.string.refresh_pulling),
            stringResource(R.string.refresh_release),
            stringResource(R.string.refresh_refreshing),
            stringResource(R.string.refresh_complete),
        ),
        miuixCollapsedSearchField = { dynamicTopPadding, placeholder ->
            AppListSearchFieldMiuix(
                label = placeholder,
                dynamicTopPadding = dynamicTopPadding,
            )
        },
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
