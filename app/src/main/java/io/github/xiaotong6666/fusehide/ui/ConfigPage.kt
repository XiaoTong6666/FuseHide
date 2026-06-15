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

import android.view.HapticFeedbackConstants
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
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R

@Composable
fun ConfigPage(
    hookStatus: HookStatusUiState,
    state: ConfigUiState,
    callbacks: ConfigCallbacks,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    scrollModifier: Modifier = Modifier,
    title: String = stringResource(R.string.app_name),
    subtitle: String = stringResource(R.string.home_subtitle_policy),
) {
    val view = LocalView.current
    val scrollState = rememberScrollState()
    var showDetailedDiff by rememberSaveable { mutableStateOf(false) }
    val resultsNeedAttention = state.highlightConfigResults || state.draftVsAppliedDiff.hasDifferences

    LaunchedEffect(state.configResultsScrollToken) {
        if (state.configResultsScrollToken > 0) {
            scrollState.animateScrollTo(scrollState.maxValue)
        }
    }

    ConfigPageContent(
        state = state,
        callbacks = callbacks,
        contentPadding = contentPadding,
        showDetailedDiff = showDetailedDiff,
        onShowDetailedDiffChanged = { showDetailedDiff = it },
        resultsNeedAttention = resultsNeedAttention,
        view = view,
        scrollModifier = scrollModifier,
    )
}

@Composable
private fun ConfigPageContent(
    state: ConfigUiState,
    callbacks: ConfigCallbacks,
    contentPadding: PaddingValues,
    showDetailedDiff: Boolean,
    onShowDetailedDiffChanged: (Boolean) -> Unit,
    resultsNeedAttention: Boolean,
    view: android.view.View,
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
            SectionTitle(stringResource(R.string.section_editable_draft))
            Spacer(Modifier.height(4.dp))
            SectionDescription(
                text = stringResource(R.string.section_editable_draft_desc),
                style = SectionDescriptionStyle.Supporting,
            )
            Spacer(Modifier.height(12.dp))
            ConfigToggleCard(
                checked = state.enableHideAllRootEntries,
                title = stringResource(R.string.field_hide_all_title),
                description = stringResource(R.string.field_hide_all_desc),
                onToggle = {
                    view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                    callbacks.onEnableHideAllRootEntriesChanged(!state.enableHideAllRootEntries)
                },
            )
            Spacer(Modifier.height(12.dp))
            ConfigTextField(
                value = state.hideAllRootEntriesExemptionsText,
                onValueChange = callbacks.onHideAllRootEntriesExemptionsChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.field_visible_exemptions),
                minLines = 5,
                maxLines = 5,
            )
            Spacer(Modifier.height(4.dp))
            SectionDescription(
                text = stringResource(R.string.field_visible_exemptions_help),
                style = SectionDescriptionStyle.Supporting,
            )
            Spacer(Modifier.height(10.dp))
            ConfigTextField(
                value = state.hiddenTargetsText,
                onValueChange = callbacks.onHiddenTargetsChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.field_hidden_targets),
                minLines = 5,
                maxLines = 5,
            )
            Spacer(Modifier.height(4.dp))
            SectionDescription(
                text = stringResource(R.string.field_hidden_targets_help),
                style = SectionDescriptionStyle.Supporting,
            )
            Spacer(Modifier.height(10.dp))
            ConfigTextField(
                value = state.hiddenPackagesText,
                onValueChange = callbacks.onHiddenPackagesChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.field_hidden_package_names),
                minLines = 5,
                maxLines = 5,
            )
            Spacer(Modifier.height(4.dp))
            SectionDescription(
                text = stringResource(R.string.field_hidden_package_names_help),
                style = SectionDescriptionStyle.Supporting,
            )
            Spacer(Modifier.height(14.dp))
            PrimaryActionButton(
                label = stringResource(R.string.button_apply),
                onClick = callbacks.onApplyConfigClick,
            )
            Spacer(Modifier.height(8.dp))
            DualActionRow(
                primaryLabel = "",
                onPrimaryClick = {},
                secondaryLabel = stringResource(R.string.button_restore_defaults),
                onSecondaryClick = callbacks.onResetConfigClick,
            )
        }

        SectionCard {
            SectionTitle(stringResource(R.string.section_apply_feedback))
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalAlignment = Alignment.Top,
            ) {
                MetricCard(stringResource(R.string.label_last_ack), state.lastAckResultText, Modifier.weight(1f))
                MetricCard(stringResource(R.string.label_applied_at), state.lastApplyTimeText, Modifier.weight(1f))
            }
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_last_token),
                text = state.lastAckTokenText,
                monospace = true,
            )
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = if (resultsNeedAttention) stringResource(R.string.label_attention) else stringResource(R.string.label_status),
                text = state.configStatusText.ifEmpty { state.draftVsAppliedDiff.summary },
                emphasized = resultsNeedAttention,
            )
            Spacer(Modifier.height(8.dp))
            InfoPanel(
                title = stringResource(R.string.label_draft_vs_applied),
                text = state.draftVsAppliedDiff.summary,
                emphasized = state.draftVsAppliedDiff.hasDifferences,
            )
        }
        if (state.draftVsAppliedDiff.hasDifferences) {
            SectionCard {
                SectionTitle(stringResource(R.string.section_detailed_diff), SectionTitleStyle.Small)
                Spacer(Modifier.height(6.dp))
                InlineTextButton(
                    label = if (showDetailedDiff) {
                        stringResource(R.string.button_hide_detailed_diff)
                    } else {
                        stringResource(R.string.button_show_detailed_diff)
                    },
                    onClick = { onShowDetailedDiffChanged(!showDetailedDiff) },
                )
                if (showDetailedDiff) {
                    Spacer(Modifier.height(6.dp))
                    InfoPanel(
                        title = stringResource(R.string.label_draft_vs_applied),
                        text = state.draftVsAppliedDiff.details,
                        monospace = true,
                    )
                }
            }
        }
    }
}
