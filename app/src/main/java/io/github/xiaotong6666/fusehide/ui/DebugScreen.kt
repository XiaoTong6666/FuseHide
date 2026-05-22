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
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R

@Composable
fun DebugScreen(
    hookStatus: HookStatusUiState,
    state: DebugUiState,
    callbacks: DebugCallbacks,
    contentPadding: PaddingValues,
) {
    val scrollState = rememberScrollState()
    var showStorageHint by remember { mutableStateOf(false) }
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
                style = MaterialTheme.typography.titleLarge,
                color = MaterialTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(6.dp))
            Text(
                text = stringResource(R.string.section_probe_target_desc),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
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
                style = MaterialTheme.typography.titleLarge,
                color = MaterialTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(12.dp))
            OutlinedTextField(
                value = state.pathText,
                onValueChange = callbacks.onPathChanged,
                modifier = Modifier.fillMaxWidth(),
                label = { Text(stringResource(R.string.label_primary_path)) },
                colors = OutlinedTextFieldDefaults.colors(
                    focusedBorderColor = MaterialTheme.colorScheme.primary,
                    unfocusedBorderColor = MaterialTheme.colorScheme.outline,
                    focusedLabelColor = MaterialTheme.colorScheme.primary,
                    unfocusedLabelColor = MaterialTheme.colorScheme.onSurfaceVariant,
                ),
                textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onSurface),
                singleLine = false,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.field_primary_path_help),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(10.dp))
            OutlinedTextField(
                value = state.pathText2,
                onValueChange = callbacks.onPath2Changed,
                modifier = Modifier.fillMaxWidth(),
                label = { Text(stringResource(R.string.label_secondary_path)) },
                colors = OutlinedTextFieldDefaults.colors(
                    focusedBorderColor = MaterialTheme.colorScheme.primary,
                    unfocusedBorderColor = MaterialTheme.colorScheme.outline,
                    focusedLabelColor = MaterialTheme.colorScheme.primary,
                    unfocusedLabelColor = MaterialTheme.colorScheme.onSurfaceVariant,
                ),
                textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onSurface),
                singleLine = false,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(R.string.field_secondary_path_help),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        SectionCard {
            Text(
                stringResource(R.string.section_common_probes),
                style = MaterialTheme.typography.titleLarge,
                color = MaterialTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(12.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = callbacks.onStatClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_stat), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                Button(onClick = callbacks.onAccessClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_access), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = callbacks.onListClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_list), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                Button(onClick = callbacks.onOpenClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_open), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }

            Spacer(Modifier.height(12.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    stringResource(R.string.section_mutation_probes),
                    style = MaterialTheme.typography.titleLarge,
                    color = MaterialTheme.colorScheme.onSurface,
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    IconButton(onClick = { showStorageHint = true }) {
                        Icon(
                            painter = painterResource(R.drawable.ic_info),
                            contentDescription = stringResource(R.string.section_mutation_probes),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Switch(
                        checked = state.manageStorageGranted,
                        onCheckedChange = { callbacks.onToggleManageStorage(it) },
                    )
                }
            }
            Spacer(Modifier.height(10.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedButton(onClick = callbacks.onGetConClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_get_con), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                OutlinedButton(onClick = callbacks.onCreateClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_create), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedButton(onClick = callbacks.onMkdirClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_mkdir), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                OutlinedButton(onClick = callbacks.onMoveClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_rename_move), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                OutlinedButton(
                    onClick = callbacks.onRmdirClick,
                    modifier = Modifier.weight(1f),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.error),
                    shape = MaterialTheme.shapes.small,
                ) {
                    Text(stringResource(R.string.button_rmdir), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                OutlinedButton(onClick = callbacks.onUnlinkClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_unlink), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }

            Spacer(Modifier.height(12.dp))
            Text(
                stringResource(R.string.section_utilities),
                style = MaterialTheme.typography.titleLarge,
                color = MaterialTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(10.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                FilledTonalButton(onClick = callbacks.onAllPkgClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_all_pkg), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                FilledTonalButton(onClick = callbacks.onInsertZwjClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_insert_zwj), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                FilledTonalButton(onClick = callbacks.onClearClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_clear_output), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                FilledTonalButton(onClick = callbacks.onResetClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_reset_path), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }
            Spacer(Modifier.height(8.dp))
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                FilledTonalButton(onClick = callbacks.onCopyAllClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_copy_output), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
                FilledTonalButton(onClick = callbacks.onSelfDataClick, modifier = Modifier.weight(1f), shape = MaterialTheme.shapes.small) {
                    Text(stringResource(R.string.button_self_data), maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MaterialTheme.typography.titleMedium)
                }
            }
        }

        SectionCard {
            Text(
                stringResource(R.string.section_probe_output),
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.onSurface,
            )
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_runtime_output),
                text = state.outputText.ifEmpty { stringResource(R.string.probe_output_empty) },
                monospace = true,
            )
        }
    }

    if (showStorageHint) {
        AlertDialog(
            onDismissRequest = { showStorageHint = false },
            title = { Text(stringResource(R.string.section_mutation_probes)) },
            text = { Text(stringResource(R.string.hint_manage_storage)) },
            dismissButton = {
                TextButton(onClick = { showStorageHint = false }) {
                    Text(stringResource(R.string.button_close))
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    showStorageHint = false
                    callbacks.onToggleManageStorage(true)
                }) {
                    Text(stringResource(R.string.button_go_settings))
                }
            },
        )
    }
}
