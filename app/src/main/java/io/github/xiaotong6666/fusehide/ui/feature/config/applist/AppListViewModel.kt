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

package io.github.xiaotong6666.fusehide.ui.feature.config.applist

import android.app.Application
import android.content.pm.PackageManager
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.AppInfo
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.GroupedApps
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.SearchStatus
import io.github.xiaotong6666.uihelper.chrome.SearchPageState.ResultStatus
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import java.text.Collator
import java.util.Locale

class AppListViewModel(application: Application) : AndroidViewModel(application) {
    private val packageManager: PackageManager = application.packageManager
    private val refreshMutex = Mutex()
    private val searchQuery = MutableStateFlow("")

    private val _uiState = MutableStateFlow(
        AppListUiState(
            searchStatus = SearchStatus(placeholder = application.getString(R.string.search_apps_placeholder)),
        ),
    )
    val uiState: StateFlow<AppListUiState> = _uiState.asStateFlow()

    private var allApps: List<AppInfo> = emptyList()

    init {
        viewModelScope.launchSearchQueryCollector(searchQuery, ::applySearchText)
    }

    fun loadAppList(force: Boolean = false): Job = viewModelScope.launch {
        refreshMutex.withLock {
            if (!force && allApps.isNotEmpty() && _uiState.value.hasLoaded) {
                return@withLock
            }

            _uiState.update { it.copy(isRefreshing = true) }

            val newApps = withContext(Dispatchers.IO) {
                packageManager.getInstalledPackages(0).mapNotNull { pkg ->
                    val appInfo = pkg.applicationInfo ?: return@mapNotNull null
                    AppInfo(
                        packageName = pkg.packageName,
                        label = appInfo.loadLabel(packageManager).toString(),
                        packageInfo = pkg,
                        applicationInfo = appInfo,
                        uid = appInfo.uid,
                    )
                }
            }

            allApps = newApps
            val userIds = newApps.map { it.uid / 100000 }.toSet()
            val groups = withContext(Dispatchers.IO) { buildGroups(newApps) }

            updateVisibleApps(groups, userIds)
        }
    }

    private fun buildGroups(apps: List<AppInfo>): List<GroupedApps> {
        val collator = Collator.getInstance(Locale.getDefault())
        return apps.groupBy { it.uid }.map { (uid, list) ->
            val sorted = list.sortedWith(compareBy(collator) { it.label })
            val primary = sorted.first()
            GroupedApps(
                uid = uid,
                primary = primary,
                apps = sorted,
            )
        }.sortedWith(compareBy(collator) { it.primary.label })
    }

    fun updateSearchText(text: String) {
        updateSearchStatus(_uiState.value.searchStatus.copy(query = text))
    }

    fun updateSearchStatus(status: SearchStatus) {
        val previous = _uiState.value.searchStatus
        _uiState.update { it.copy(searchStatus = status) }
        if (previous.query != status.query) {
            searchQuery.value = status.query
        }
    }

    private fun filterSearchResults(groups: List<GroupedApps>, text: String): List<GroupedApps> {
        if (text.isEmpty()) return emptyList()

        return groups.mapNotNull { group ->
            val matchedPackageNames = group.apps.filter {
                it.label.contains(text, ignoreCase = true) ||
                    it.packageName.contains(text, ignoreCase = true)
            }.mapTo(linkedSetOf()) { it.packageName }

            if (matchedPackageNames.isEmpty()) {
                null
            } else {
                val sortedApps = group.apps.sortedWith(
                    compareByDescending { it.packageName in matchedPackageNames },
                )
                group.copy(
                    apps = sortedApps,
                    matchedPackageNames = matchedPackageNames,
                )
            }
        }
    }

    private suspend fun applySearchText(text: String) {
        _uiState.update {
            it.copy(
                searchStatus = it.searchStatus.copy(resultStatus = searchLoadingStatusFor(text)),
            )
        }

        if (text.isEmpty()) {
            _uiState.update { state ->
                state.copy(
                    searchResults = emptyList(),
                    searchStatus = state.searchStatus.copy(resultStatus = ResultStatus.DEFAULT),
                )
            }
            return
        }

        val result = withContext(Dispatchers.IO) {
            filterSearchResults(_uiState.value.groupedApps, text)
        }

        _uiState.update {
            it.copy(
                searchResults = result,
                searchStatus = it.searchStatus.copy(resultStatus = searchResultStatusFor(text, result.isEmpty())),
            )
        }
    }

    private suspend fun updateVisibleApps(groups: List<GroupedApps>, userIds: Set<Int>) {
        val searchText = _uiState.value.searchStatus.query
        val searchResults = withContext(Dispatchers.IO) {
            filterSearchResults(groups, searchText)
        }
        _uiState.update {
            it.copy(
                isRefreshing = false,
                hasLoaded = true,
                groupedApps = groups,
                searchResults = searchResults,
                userIds = userIds,
                searchStatus = it.searchStatus.copy(
                    resultStatus = searchResultStatusFor(searchText, searchResults.isEmpty()),
                ),
            )
        }
    }
}
