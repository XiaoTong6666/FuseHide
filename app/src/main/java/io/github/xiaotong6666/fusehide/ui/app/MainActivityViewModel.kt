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

package io.github.xiaotong6666.fusehide.ui.app

import androidx.compose.runtime.Immutable
import androidx.lifecycle.ViewModel
import io.github.xiaotong6666.fusehide.config.HideConfig
import io.github.xiaotong6666.fusehide.config.HideConfigDefaults
import io.github.xiaotong6666.fusehide.debug.PathDebugActions
import io.github.xiaotong6666.fusehide.ui.core.model.HookBackend
import io.github.xiaotong6666.uihelper.mode.UiMode
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import java.util.UUID

@Immutable
data class MainActivityUiState(
    val selectedTab: Int = 0,
    val uiMode: UiMode = UiMode.Miuix,
    val enableMiuixBlur: Boolean = false,
    val enableMiuixFloatingBottomBar: Boolean = false,
    val infoText: String = "",
    val statusText: String = "",
    val hookBackend: HookBackend? = null,
    val hookedPackage: String? = null,
    val hookedPid: Int = -1,
    val hookCheckCompleted: Boolean = false,
    val configStatusText: String = "",
    val lastAckTokenText: String = "-",
    val lastAckResultText: String = "-",
    val lastApplyTimeText: String = "-",
    val appliedHideConfig: HideConfig? = null,
    val appliedConfigSnapshotText: String = "",
    val appliedConfigQueryPending: Boolean = false,
    val highlightConfigResults: Boolean = false,
    val localConfigMissing: Boolean = false,
    val defaultSaveExplicitlyRequested: Boolean = false,
    val configResultsScrollToken: Int = 0,
    val shouldAutoScrollConfigResults: Boolean = false,
    val currentHideConfig: HideConfig = HideConfigDefaults.value,
    val pathText: String = PathDebugActions.defaultPath(),
    val pathText2: String = "",
    val outputText: String = "",
)

class MainActivityViewModel : ViewModel() {
    private val _uiState = MutableStateFlow(MainActivityUiState())
    val uiState: StateFlow<MainActivityUiState> = _uiState.asStateFlow()

    private var initialized = false
    private var pendingReloadToken: String? = null
    private var pendingQueryToken: String? = null

    fun initialize(
        uiMode: UiMode,
        enableMiuixBlur: Boolean,
        enableMiuixFloatingBottomBar: Boolean,
        infoText: String,
        savedConfig: HideConfig?,
        configStatusText: String,
        appliedConfigSnapshotText: String,
    ) {
        if (initialized) return
        initialized = true
        _uiState.update {
            it.copy(
                uiMode = uiMode,
                enableMiuixBlur = enableMiuixBlur,
                enableMiuixFloatingBottomBar = enableMiuixFloatingBottomBar,
                infoText = infoText,
                currentHideConfig = savedConfig ?: HideConfigDefaults.value,
                localConfigMissing = savedConfig == null,
                highlightConfigResults = savedConfig == null,
                configStatusText = configStatusText,
                appliedConfigSnapshotText = appliedConfigSnapshotText,
            )
        }
    }

    fun setSelectedTab(index: Int) = update { copy(selectedTab = index) }

    fun setUiMode(mode: UiMode) = update { copy(uiMode = mode) }

    fun setMiuixBlurEnabled(enabled: Boolean) = update { copy(enableMiuixBlur = enabled) }

    fun setMiuixFloatingBottomBarEnabled(enabled: Boolean) = update { copy(enableMiuixFloatingBottomBar = enabled) }

    fun beginHookStatusCheck(statusText: String) = update {
        copy(
            statusText = statusText,
            hookCheckCompleted = false,
        )
    }

    fun completeHookStatusCheck(statusText: String) = update {
        copy(
            statusText = statusText,
            hookBackend = null,
            hookedPackage = null,
            hookedPid = -1,
            hookCheckCompleted = true,
        )
    }

    fun setHookedStatus(packageName: String, pid: Int, backend: HookBackend?, statusText: String) = update {
        copy(
            statusText = statusText,
            hookBackend = backend,
            hookedPackage = packageName,
            hookedPid = pid,
            hookCheckCompleted = true,
        )
    }

    fun updateConfig(config: HideConfig) = update { copy(currentHideConfig = config) }

    fun beginConfigReload(statusText: String): String {
        val token = UUID.randomUUID().toString()
        pendingReloadToken = token
        update {
            copy(
                localConfigMissing = false,
                defaultSaveExplicitlyRequested = false,
                configStatusText = statusText,
            )
        }
        return token
    }

    fun consumeReloadAck(token: String?): Boolean {
        if (token == null || token != pendingReloadToken) return false
        pendingReloadToken = null
        return true
    }

    fun markConfigRestoredDefaults(statusText: String) = update {
        copy(
            currentHideConfig = HideConfigDefaults.value,
            defaultSaveExplicitlyRequested = true,
            configStatusText = statusText,
        )
    }

    fun markDefaultOverwriteRefused(statusText: String) = update {
        copy(
            configStatusText = statusText,
            highlightConfigResults = true,
            configResultsScrollToken = configResultsScrollToken + 1,
        )
    }

    fun applyReloadAck(
        token: String,
        resultText: String,
        applyTimeText: String,
        applied: Boolean,
        statusText: String,
    ) = update {
        copy(
            lastAckTokenText = token,
            lastAckResultText = resultText,
            lastApplyTimeText = applyTimeText,
            highlightConfigResults = !applied,
            configStatusText = statusText,
            shouldAutoScrollConfigResults = !applied || shouldAutoScrollConfigResults,
            configResultsScrollToken = if (applied) configResultsScrollToken else configResultsScrollToken + 1,
        )
    }

    fun beginAppliedConfigQuery(snapshotText: String, autoScrollToResults: Boolean): String {
        val token = UUID.randomUUID().toString()
        pendingQueryToken = token
        update {
            copy(
                appliedConfigSnapshotText = snapshotText,
                appliedConfigQueryPending = true,
                shouldAutoScrollConfigResults = autoScrollToResults,
            )
        }
        return token
    }

    fun consumeAppliedConfigResult(token: String?): Boolean {
        if (token == null || token != pendingQueryToken) return false
        pendingQueryToken = null
        return true
    }

    fun applyAppliedConfig(
        config: HideConfig?,
        snapshotText: String,
        draftDiffers: Boolean,
    ) = update {
        val shouldScroll = shouldAutoScrollConfigResults
        copy(
            appliedHideConfig = config,
            appliedConfigSnapshotText = snapshotText,
            appliedConfigQueryPending = false,
            highlightConfigResults = config == null || draftDiffers,
            configResultsScrollToken = if (shouldScroll) configResultsScrollToken + 1 else configResultsScrollToken,
            shouldAutoScrollConfigResults = false,
        )
    }

    fun timeoutAppliedConfigQuery(token: String, snapshotText: String): Boolean {
        if (token != pendingQueryToken) return false
        pendingQueryToken = null
        update {
            copy(
                appliedHideConfig = null,
                appliedConfigSnapshotText = snapshotText,
                appliedConfigQueryPending = false,
                highlightConfigResults = true,
                shouldAutoScrollConfigResults = false,
            )
        }
        return true
    }

    fun recoverConfig(config: HideConfig) = update { copy(currentHideConfig = config) }

    fun setPathText(text: String) = update { copy(pathText = text) }

    fun setPathText2(text: String) = update { copy(pathText2 = text) }

    fun setDebugPaths(path: String, path2: String) = update {
        copy(
            selectedTab = 2,
            pathText = path,
            pathText2 = path2,
        )
    }

    fun clearOutput() = update { copy(outputText = "") }

    fun setOutput(text: String) = update { copy(outputText = text) }

    fun appendOutput(text: String) = update { copy(outputText = outputText + text) }

    fun appendZeroWidthJoiner() = update { copy(pathText = pathText + "\\u200d") }

    fun resetDebugPath() = update { copy(pathText = PathDebugActions.defaultPath()) }

    private inline fun update(transform: MainActivityUiState.() -> MainActivityUiState) {
        _uiState.update(transform)
    }
}
