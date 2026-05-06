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

package io.github.xiaotong6666.fusehide.ui

data class HideConfigDiff(
    val hasDifferences: Boolean,
    val summary: String,
    val details: String,
)

data class GridActionItem(
    val label: String,
    val action: () -> Unit,
    val isError: Boolean = false,
)

data class HookStatusUiState(
    val infoText: String,
    val statusText: String,
    val isHooked: Boolean,
    val hookedPackage: String?,
    val hookedPid: Int,
    val hookCheckCompleted: Boolean,
)

data class ConfigUiState(
    val configStatusText: String,
    val lastAckTokenText: String,
    val lastAckResultText: String,
    val lastApplyTimeText: String,
    val draftVsAppliedDiff: HideConfigDiff,
    val appliedConfigSnapshotText: String,
    val highlightConfigResults: Boolean,
    val configResultsScrollToken: Int,
    val enableHideAllRootEntries: Boolean,
    val hideAllRootEntriesExemptionsText: String,
    val hiddenTargetsText: String,
    val hiddenPackagesText: String,
)

data class DebugUiState(
    val pathText: String,
    val pathText2: String,
    val outputText: String,
)

data class ConfigCallbacks(
    val onStatusClick: () -> Unit,
    val onEnableHideAllRootEntriesChanged: (Boolean) -> Unit,
    val onHideAllRootEntriesExemptionsChanged: (String) -> Unit,
    val onHiddenTargetsChanged: (String) -> Unit,
    val onHiddenPackagesChanged: (String) -> Unit,
    val onSaveConfigClick: () -> Unit,
    val onApplyConfigClick: () -> Unit,
    val onResetConfigClick: () -> Unit,
    val onRefreshAppliedConfigClick: () -> Unit,
)

data class DebugCallbacks(
    val onStatusClick: () -> Unit,
    val onPathChanged: (String) -> Unit,
    val onPath2Changed: (String) -> Unit,
    val onStatClick: () -> Unit,
    val onAccessClick: () -> Unit,
    val onListClick: () -> Unit,
    val onOpenClick: () -> Unit,
    val onGetConClick: () -> Unit,
    val onCreateClick: () -> Unit,
    val onMkdirClick: () -> Unit,
    val onMoveClick: () -> Unit,
    val onRmdirClick: () -> Unit,
    val onUnlinkClick: () -> Unit,
    val onAllPkgClick: () -> Unit,
    val onInsertZwjClick: () -> Unit,
    val onClearClick: () -> Unit,
    val onResetClick: () -> Unit,
    val onCopyAllClick: () -> Unit,
    val onSelfDataClick: () -> Unit,
)
