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
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R

@Composable
fun SettingsPage(
    state: SettingsUiState,
    callbacks: SettingsCallbacks,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    scrollModifier: Modifier = Modifier,
    title: String = stringResource(R.string.app_name),
    subtitle: String = stringResource(R.string.home_subtitle_settings),
) {
    SettingsPageContent(state, callbacks, contentPadding, scrollModifier)
}

@Composable
private fun SettingsPageContent(
    state: SettingsUiState,
    callbacks: SettingsCallbacks,
    contentPadding: PaddingValues,
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
            SectionTitle(stringResource(R.string.section_ui_family))
            Spacer(Modifier.height(6.dp))
            SectionDescription(
                text = stringResource(R.string.section_ui_family_desc),
                style = SectionDescriptionStyle.Supporting,
            )
            Spacer(Modifier.height(12.dp))
            ConfigToggleCard(
                checked = state.uiMode == UiMode.Miuix,
                title = stringResource(R.string.field_use_miuix_title),
                description = if (state.uiMode == UiMode.Miuix) {
                    stringResource(R.string.field_use_miuix_desc_enabled)
                } else {
                    stringResource(R.string.field_use_miuix_desc_disabled)
                },
                onToggle = callbacks.onToggleUiMode,
            )
        }

        SectionCard {
            SectionTitle(stringResource(R.string.section_about), SectionTitleStyle.Medium)
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.label_current_ui_family),
                text = if (state.uiMode == UiMode.Miuix) {
                    stringResource(R.string.value_ui_family_miuix)
                } else {
                    stringResource(R.string.value_ui_family_material)
                },
            )
            Spacer(Modifier.height(10.dp))
            InfoPanel(
                title = stringResource(R.string.app_name),
                text = stringResource(R.string.app_description),
            )
        }
    }
}
