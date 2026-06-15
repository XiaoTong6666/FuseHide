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

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R

@Composable
fun HomePage(
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    callbacks: HomeCallbacks,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    scrollModifier: Modifier = Modifier,
    title: String = stringResource(R.string.app_name),
    subtitle: String = stringResource(R.string.home_subtitle_runtime),
) {
    val resultsNeedAttention = configState.highlightConfigResults || configState.draftVsAppliedDiff.hasDifferences

    HomePageContent(
        hookStatus = hookStatus,
        configState = configState,
        callbacks = callbacks,
        contentPadding = contentPadding,
        isCurrentPage = isCurrentPage,
        resultsNeedAttention = resultsNeedAttention,
        scrollModifier = scrollModifier,
    )
}

@Composable
private fun HomePageContent(
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    callbacks: HomeCallbacks,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean,
    resultsNeedAttention: Boolean,
    scrollModifier: Modifier,
) {
    val scrollState = rememberScrollState()
    Column(
        modifier = Modifier
            .fillMaxSize()
            .then(scrollModifier)
            .verticalScroll(scrollState)
            .padding(contentPadding)
            .padding(horizontal = 16.dp, vertical = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard {
            SectionTitle(stringResource(R.string.section_runtime_overview))
            Spacer(Modifier.height(6.dp))
            SectionDescription(
                text = stringResource(R.string.section_runtime_overview_desc),
                style = SectionDescriptionStyle.Supporting,
            )
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                StatusChip(
                    modifier = Modifier.weight(1f).heightIn(min = 130.dp),
                    label = stringResource(R.string.label_hook),
                    value = hookSummaryValue(isHooked = hookStatus.isHooked, hookCheckCompleted = hookStatus.hookCheckCompleted),
                    supportingText = hookSummarySupportingText(
                        isHooked = hookStatus.isHooked,
                        hookCheckCompleted = hookStatus.hookCheckCompleted,
                        hookedPackage = hookStatus.hookedPackage,
                    ),
                    metaText = hookSummaryMetaText(isHooked = hookStatus.isHooked, hookedPid = hookStatus.hookedPid),
                    emphasized = hookStatus.isHooked,
                    onClick = callbacks.onStatusClick,
                )
                StatusChip(
                    modifier = Modifier.weight(1f).heightIn(min = 130.dp),
                    label = stringResource(R.string.label_sync),
                    value = if (resultsNeedAttention) stringResource(R.string.state_sync_needs_review) else stringResource(R.string.state_sync_ok),
                    supportingText = configState.draftVsAppliedDiff.summary,
                    emphasized = !resultsNeedAttention,
                )
            }
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_runtime_summary),
                text = configState.configStatusText.ifEmpty { configState.draftVsAppliedDiff.summary },
                emphasized = resultsNeedAttention,
            )
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_current_native_config),
                text = configState.appliedConfigSnapshotText,
                monospace = true,
                emphasized = resultsNeedAttention,
            )
        }

        SectionCard {
            SectionTitle(stringResource(R.string.section_device_status), SectionTitleStyle.Medium)
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_device),
                text = hookStatus.infoText,
                monospace = true,
            )
        }
    }
}
