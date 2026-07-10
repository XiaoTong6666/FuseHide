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

import io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets.SearchStatus
import io.github.xiaotong6666.uihelper.chrome.SearchPageState
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.FlowPreview
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.debounce
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import kotlin.time.Duration.Companion.milliseconds

private const val SEARCH_DEBOUNCE_MILLIS = 150L

@OptIn(FlowPreview::class)
fun CoroutineScope.launchSearchQueryCollector(
    searchQuery: StateFlow<String>,
    onQuery: suspend (String) -> Unit,
): Job = launch {
    searchQuery
        .debounce(SEARCH_DEBOUNCE_MILLIS.milliseconds)
        .distinctUntilChanged()
        .collectLatest(onQuery)
}

fun searchLoadingStatusFor(text: String): SearchPageState.ResultStatus = if (text.isEmpty()) {
    SearchPageState.ResultStatus.DEFAULT
} else {
    SearchPageState.ResultStatus.LOAD
}

fun searchResultStatusFor(text: String, isEmpty: Boolean): SearchPageState.ResultStatus = when {
    text.isEmpty() -> SearchPageState.ResultStatus.DEFAULT
    isEmpty -> SearchPageState.ResultStatus.EMPTY
    else -> SearchPageState.ResultStatus.SHOW
}
