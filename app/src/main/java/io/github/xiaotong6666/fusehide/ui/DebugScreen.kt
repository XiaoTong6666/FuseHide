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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun DebugScreen(
    hookStatus: HookStatusUiState,
    state: DebugUiState,
    callbacks: DebugCallbacks,
    contentPadding: PaddingValues,
) {
    val scrollState = rememberScrollState()
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
                stringResource(R.string.section_probe_target),
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(6.dp))
            Text(
                text = stringResource(R.string.section_probe_target_desc),
                style = MiuixTheme.textStyles.body1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            )
            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                StatusChip(
                    modifier = Modifier.weight(1f),
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
            }
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_path),
                text = state.pathText.ifBlank { "-" },
                monospace = true,
            )
            Spacer(Modifier.height(10.dp))
            InfoPanel(title = stringResource(R.string.label_device), text = hookStatus.infoText, monospace = true)
        }

        SectionCard {
            Text(
                stringResource(R.string.section_paths),
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(12.dp))
            TextField(
                value = state.pathText,
                onValueChange = callbacks.onPathChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.label_primary_path),
                backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                borderColor = MiuixTheme.colorScheme.primary,
                textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurface),
                singleLine = false,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.field_primary_path_help),
                style = MiuixTheme.textStyles.footnote1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            )
            Spacer(Modifier.height(10.dp))
            TextField(
                value = state.pathText2,
                onValueChange = callbacks.onPath2Changed,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.label_secondary_path),
                backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                borderColor = MiuixTheme.colorScheme.primary,
                textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurface),
                singleLine = false,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.field_secondary_path_help),
                style = MiuixTheme.textStyles.footnote1,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            )
        }

        SectionCard {
            Text(
                stringResource(R.string.section_common_probes),
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(12.dp))
            ActionGrid(
                listOf(
                    GridActionItem(stringResource(R.string.button_stat), callbacks.onStatClick),
                    GridActionItem(stringResource(R.string.button_access), callbacks.onAccessClick),
                    GridActionItem(stringResource(R.string.button_list), callbacks.onListClick),
                    GridActionItem(stringResource(R.string.button_open), callbacks.onOpenClick),
                ),
            )
            Spacer(Modifier.height(12.dp))
            Text(
                stringResource(R.string.section_mutation_probes),
                style = MiuixTheme.textStyles.title4.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(10.dp))
            ActionGrid(
                listOf(
                    GridActionItem(stringResource(R.string.button_get_con), callbacks.onGetConClick),
                    GridActionItem(stringResource(R.string.button_create), callbacks.onCreateClick),
                    GridActionItem(stringResource(R.string.button_mkdir), callbacks.onMkdirClick),
                    GridActionItem(stringResource(R.string.button_rename_move), callbacks.onMoveClick),
                    GridActionItem(stringResource(R.string.button_rmdir), callbacks.onRmdirClick, isError = true),
                    GridActionItem(stringResource(R.string.button_unlink), callbacks.onUnlinkClick),
                ),
            )
            Spacer(Modifier.height(12.dp))
            Text(
                stringResource(R.string.section_utilities),
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(10.dp))
            ActionGrid(
                listOf(
                    GridActionItem(stringResource(R.string.button_all_pkg), callbacks.onAllPkgClick),
                    GridActionItem(stringResource(R.string.button_insert_zwj), callbacks.onInsertZwjClick),
                    GridActionItem(stringResource(R.string.button_clear_output), callbacks.onClearClick),
                    GridActionItem(stringResource(R.string.button_reset_path), callbacks.onResetClick),
                    GridActionItem(stringResource(R.string.button_copy_output), callbacks.onCopyAllClick),
                    GridActionItem(stringResource(R.string.button_self_data), callbacks.onSelfDataClick),
                ),
            )
        }

        SectionCard {
            Text(
                stringResource(R.string.section_probe_output),
                style = MiuixTheme.textStyles.title4.copy(fontWeight = FontWeight.Medium),
                color = MiuixTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_runtime_output),
                text = state.outputText.ifEmpty { stringResource(R.string.probe_output_empty) },
                monospace = true,
            )
        }
    }
}
