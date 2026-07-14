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

package io.github.xiaotong6666.fusehide.ui.feature.debug

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
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
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.core.model.DebugCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.DebugUiState
import io.github.xiaotong6666.uihelper.adaptive.ActionGrid
import io.github.xiaotong6666.uihelper.adaptive.AppTextField
import io.github.xiaotong6666.uihelper.adaptive.InfoPanel
import io.github.xiaotong6666.uihelper.adaptive.SectionCard
import io.github.xiaotong6666.uihelper.adaptive.SectionDescription
import io.github.xiaotong6666.uihelper.adaptive.SectionTitle
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import io.github.xiaotong6666.uihelper.model.GridActionItem
import io.github.xiaotong6666.uihelper.model.GridActionStyle
import io.github.xiaotong6666.uihelper.model.SectionDescriptionStyle
import io.github.xiaotong6666.uihelper.model.SectionTitleStyle
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun DebugPage(
    state: DebugUiState,
    callbacks: DebugCallbacks,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    modifier: Modifier = Modifier,
) {
    DebugPageContent(state, callbacks, contentPadding, modifier)
}

@Composable
private fun DebugPageContent(
    state: DebugUiState,
    callbacks: DebugCallbacks,
    contentPadding: PaddingValues,
    modifier: Modifier,
) {
    val scrollState = rememberScrollState()
    val miuixScrollFeedbackModifier = if (LocalUiMode.current == UiMode.Miuix) {
        Modifier.scrollEndHaptic().overScrollVertical()
    } else {
        Modifier
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .then(modifier)
            .then(miuixScrollFeedbackModifier)
            .verticalScroll(scrollState)
            .padding(contentPadding)
            .padding(horizontal = 16.dp, vertical = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        SectionCard {
            SectionTitle(stringResource(R.string.section_paths))
            Spacer(Modifier.height(12.dp))
            AppTextField(
                value = state.pathText,
                onValueChange = callbacks.onPathChanged,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.label_primary_path),
            )
            Spacer(Modifier.height(4.dp))
            SectionDescription(
                text = stringResource(R.string.field_primary_path_help),
                style = SectionDescriptionStyle.Supporting,
            )
            Spacer(Modifier.height(10.dp))
            AppTextField(
                value = state.pathText2,
                onValueChange = callbacks.onPath2Changed,
                modifier = Modifier.fillMaxWidth(),
                label = stringResource(R.string.label_secondary_path),
            )
            Spacer(Modifier.height(4.dp))
            SectionDescription(
                text = stringResource(R.string.field_secondary_path_help),
                style = SectionDescriptionStyle.Supporting,
            )
        }

        SectionCard {
            SectionTitle(stringResource(R.string.section_common_probes))
            Spacer(Modifier.height(12.dp))
            ActionGrid(
                listOf(
                    GridActionItem(stringResource(R.string.button_stat), callbacks.onStatClick, GridActionStyle.Filled),
                    GridActionItem(stringResource(R.string.button_access), callbacks.onAccessClick, GridActionStyle.Filled),
                    GridActionItem(stringResource(R.string.button_list), callbacks.onListClick, GridActionStyle.Filled),
                    GridActionItem(stringResource(R.string.button_open), callbacks.onOpenClick, GridActionStyle.Filled),
                ),
            )
            Spacer(Modifier.height(12.dp))
            SectionTitle(stringResource(R.string.section_mutation_probes), SectionTitleStyle.Subsection)
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
            SectionTitle(stringResource(R.string.section_utilities))
            Spacer(Modifier.height(10.dp))
            ActionGrid(
                listOf(
                    GridActionItem(stringResource(R.string.button_all_pkg), callbacks.onAllPkgClick, GridActionStyle.Tonal),
                    GridActionItem(stringResource(R.string.button_insert_zwj), callbacks.onInsertZwjClick, GridActionStyle.Tonal),
                    GridActionItem(stringResource(R.string.button_clear_output), callbacks.onClearClick, GridActionStyle.Tonal),
                    GridActionItem(stringResource(R.string.button_reset_path), callbacks.onResetClick, GridActionStyle.Tonal),
                    GridActionItem(stringResource(R.string.button_copy_output), callbacks.onCopyAllClick, GridActionStyle.Tonal),
                    GridActionItem(stringResource(R.string.button_self_data), callbacks.onSelfDataClick, GridActionStyle.Tonal),
                ),
            )
        }

        SectionCard {
            SectionTitle(stringResource(R.string.section_probe_output), SectionTitleStyle.EmphasizedMedium)
        }

        InfoPanel(
            title = stringResource(R.string.label_runtime_output),
            text = state.outputText.ifEmpty { stringResource(R.string.probe_output_empty) },
            monospace = true,
        )
    }
}
