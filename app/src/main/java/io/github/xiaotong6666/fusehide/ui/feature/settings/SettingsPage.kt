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

package io.github.xiaotong6666.fusehide.ui.feature.settings

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.rounded.Dashboard
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.core.model.SettingsCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.SettingsUiState
import io.github.xiaotong6666.uihelper.adaptive.SettingsDropdownItem
import io.github.xiaotong6666.uihelper.adaptive.SettingsGroup
import io.github.xiaotong6666.uihelper.adaptive.SettingsGroupDivider
import io.github.xiaotong6666.uihelper.adaptive.SettingsGroupHeader
import io.github.xiaotong6666.uihelper.adaptive.SettingsInfoItem
import io.github.xiaotong6666.uihelper.adaptive.SettingsNavigationItem
import io.github.xiaotong6666.uihelper.adaptive.SettingsToggleItem
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun SettingsPage(
    state: SettingsUiState,
    callbacks: SettingsCallbacks,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    modifier: Modifier = Modifier,
) {
    SettingsPageContent(state, callbacks, contentPadding, modifier)
}

@Composable
private fun SettingsPageContent(
    state: SettingsUiState,
    callbacks: SettingsCallbacks,
    contentPadding: PaddingValues,
    modifier: Modifier,
) {
    val scrollState = rememberScrollState()
    val groupHorizontalPadding = if (LocalUiMode.current == UiMode.Miuix) 12.dp else 16.dp
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
            .padding(vertical = 16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Column {
            SettingsGroupHeader(stringResource(R.string.section_ui_family))
            Box(modifier = Modifier.padding(horizontal = groupHorizontalPadding)) {
                SettingsGroup {
                    SettingsDropdownItem(
                        title = stringResource(R.string.section_ui_family),
                        icon = Icons.Filled.Palette,
                        description = if (state.uiMode == UiMode.Miuix) {
                            stringResource(R.string.field_use_miuix_desc_enabled)
                        } else {
                            stringResource(R.string.field_use_miuix_desc_disabled)
                        },
                        items = listOf(
                            stringResource(R.string.value_ui_family_miuix),
                            stringResource(R.string.value_ui_family_material),
                        ),
                        selectedIndex = if (state.uiMode == UiMode.Miuix) 0 else 1,
                        onItemSelected = { index ->
                            val wantsMiuix = index == 0
                            if (wantsMiuix != (state.uiMode == UiMode.Miuix)) {
                                callbacks.onToggleUiMode()
                            }
                        },
                    )
                }
            }
        }

        Column {
            SettingsGroupHeader(stringResource(R.string.section_about))
            Box(modifier = Modifier.padding(horizontal = groupHorizontalPadding)) {
                SettingsGroup {
                    SettingsInfoItem(
                        title = stringResource(R.string.label_current_ui_family),
                        icon = Icons.Rounded.Dashboard,
                        value = if (state.uiMode == UiMode.Miuix) {
                            stringResource(R.string.value_ui_family_miuix)
                        } else {
                            stringResource(R.string.value_ui_family_material)
                        },
                    )
                    SettingsGroupDivider()
                    SettingsNavigationItem(
                        title = stringResource(R.string.app_name),
                        icon = Icons.Filled.Info,
                        description = stringResource(R.string.app_description),
                        onClick = {},
                    )
                }
            }
        }
    }
}
