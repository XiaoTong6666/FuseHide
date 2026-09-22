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

import androidx.compose.runtime.Stable
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.GroupedApps
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.SearchStatus

@Stable
data class AppListUiState(
    val isRefreshing: Boolean = false,
    val hasLoaded: Boolean = false,
    val groupedApps: List<GroupedApps> = emptyList(),
    val searchResults: List<GroupedApps> = emptyList(),
    val searchStatus: SearchStatus = SearchStatus(placeholder = ""),
    val userIds: Set<Int> = emptySet(),
)
