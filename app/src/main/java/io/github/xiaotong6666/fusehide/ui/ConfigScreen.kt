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
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.state.ToggleableState
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Checkbox
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun ConfigScreen(
    hookStatus: HookStatusUiState,
    state: ConfigUiState,
    callbacks: ConfigCallbacks,
    contentPadding: PaddingValues,
) {
    val view = LocalView.current
    val scrollState = rememberScrollState()
    var showDetailedDiff by remember { mutableStateOf(false) }
    var showAppliedSnapshot by remember { mutableStateOf(false) }
    val resultsNeedAttention = state.highlightConfigResults || state.draftVsAppliedDiff.hasDifferences

    LaunchedEffect(state.configResultsScrollToken) {
        if (state.configResultsScrollToken > 0) {
            scrollState.animateScrollTo(scrollState.maxValue)
        }
    }

    LaunchedEffect(state.highlightConfigResults, state.draftVsAppliedDiff.hasDifferences) {
        if (state.highlightConfigResults || state.draftVsAppliedDiff.hasDifferences) {
            showAppliedSnapshot = true
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(scrollState)
            .padding(contentPadding)
            .padding(horizontal = 16.dp, vertical = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard {
            Text(
                stringResource(R.string.section_runtime_policy),
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(6.dp))
            Text(
                text = stringResource(R.string.section_runtime_policy_desc),
                style = MiuixTheme.textStyles.footnote1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
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
                    emphasized = !resultsNeedAttention,
                )
            }
            Spacer(Modifier.height(10.dp))
            InfoPanel(title = stringResource(R.string.label_device), text = hookStatus.infoText, monospace = true)
        }

        SectionCard {
            Text(
                stringResource(R.string.section_editable_draft),
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.section_editable_draft_desc),
                style = MiuixTheme.textStyles.footnote1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            )
            Spacer(Modifier.height(12.dp))
            Card(
                onClick = {
                    view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                    callbacks.onEnableHideAllRootEntriesChanged(!state.enableHideAllRootEntries)
                },
                colors = CardDefaults.defaultColors(
                    color = MiuixTheme.colorScheme.surfaceContainerHighest,
                    contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest,
                ),
                insideMargin = PaddingValues(0.dp),
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(14.dp),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    verticalAlignment = androidx.compose.ui.Alignment.Top,
                ) {
                    Checkbox(
                        state = if (state.enableHideAllRootEntries) ToggleableState.On else ToggleableState.Off,
                        onClick = {
                            view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                            callbacks.onEnableHideAllRootEntriesChanged(!state.enableHideAllRootEntries)
                        },
                    )
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        Text(stringResource(R.string.field_hide_all_title), style = MiuixTheme.textStyles.headline2)
                        Text(
                            text = stringResource(R.string.field_hide_all_desc),
                            style = MiuixTheme.textStyles.footnote1,
                            color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                        )
                    }
                }
            }
            Spacer(Modifier.height(12.dp))
            TextField(
                value = state.hideAllRootEntriesExemptionsText,
                onValueChange = callbacks.onHideAllRootEntriesExemptionsChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.field_visible_exemptions),
                backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                borderColor = MiuixTheme.colorScheme.primary,
                textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurfaceSecondary),
                minLines = 5,
                maxLines = 5,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.field_visible_exemptions_help),
                style = MiuixTheme.textStyles.footnote1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            )
            Spacer(Modifier.height(10.dp))
            TextField(
                value = state.hiddenTargetsText,
                onValueChange = callbacks.onHiddenTargetsChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.field_hidden_targets),
                backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                borderColor = MiuixTheme.colorScheme.primary,
                textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurfaceSecondary),
                minLines = 5,
                maxLines = 5,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.field_hidden_targets_help),
                style = MiuixTheme.textStyles.footnote1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            )
            Spacer(Modifier.height(10.dp))
            TextField(
                value = state.hiddenPackagesText,
                onValueChange = callbacks.onHiddenPackagesChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.field_hidden_package_names),
                backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                borderColor = MiuixTheme.colorScheme.primary,
                textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurfaceSecondary),
                minLines = 5,
                maxLines = 5,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.field_hidden_package_names_help),
                style = MiuixTheme.textStyles.footnote1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            )
            Spacer(Modifier.height(14.dp))
            DualActionRow(
                primaryLabel = stringResource(R.string.button_apply),
                onPrimaryClick = callbacks.onApplyConfigClick,
                secondaryLabel = stringResource(R.string.button_save),
                onSecondaryClick = callbacks.onSaveConfigClick,
            )
            Spacer(Modifier.height(8.dp))
            DualActionRow(
                primaryLabel = stringResource(R.string.button_refresh_applied),
                onPrimaryClick = callbacks.onRefreshAppliedConfigClick,
                secondaryLabel = stringResource(R.string.button_restore_defaults),
                onSecondaryClick = callbacks.onResetConfigClick,
                primaryFilled = false,
            )
        }

        SectionCard {
            Text(
                stringResource(R.string.section_apply_feedback),
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalAlignment = androidx.compose.ui.Alignment.Top,
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
                Text(
                    stringResource(R.string.section_detailed_diff),
                    style = MiuixTheme.textStyles.title4,
                    color = MiuixTheme.colorScheme.onSurface,
                )
                Spacer(Modifier.height(6.dp))
                TextButton(
                    text = if (showDetailedDiff) stringResource(R.string.button_hide_detailed_diff) else stringResource(R.string.button_show_detailed_diff),
                    onClick = { showDetailedDiff = !showDetailedDiff },
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

        SectionCard {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(4.dp),
                ) {
                    Text(
                        stringResource(R.string.section_snapshot),
                        style = MiuixTheme.textStyles.title4,
                        color = MiuixTheme.colorScheme.onSurface,
                    )
                    Text(
                        text = stringResource(R.string.section_snapshot_desc),
                        style = MiuixTheme.textStyles.footnote1,
                        color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                    )
                }
                TextButton(
                    text = if (showAppliedSnapshot) stringResource(R.string.button_hide) else stringResource(R.string.button_show),
                    onClick = {
                        view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                        showAppliedSnapshot = !showAppliedSnapshot
                    },
                )
            }
            if (showAppliedSnapshot) {
                Spacer(Modifier.height(10.dp))
                InfoPanel(
                    title = stringResource(R.string.label_current_native_config),
                    text = state.appliedConfigSnapshotText,
                    monospace = true,
                    emphasized = resultsNeedAttention,
                )
            }
        }
    }
}
