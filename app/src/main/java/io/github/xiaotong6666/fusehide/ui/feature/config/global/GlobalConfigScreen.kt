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

package io.github.xiaotong6666.fusehide.ui.feature.config.global

import android.view.HapticFeedbackConstants
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.adapter.defaultGlobalConfigDrafts
import io.github.xiaotong6666.fusehide.ui.adapter.globalConfigDrafts
import io.github.xiaotong6666.fusehide.ui.adapter.updatedConfigForHiddenPackages
import io.github.xiaotong6666.fusehide.ui.adapter.updatedConfigForHiddenTargets
import io.github.xiaotong6666.fusehide.ui.adapter.updatedConfigForVisibleExemptions
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.AppConfigPageScaffold
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.AppConfigTargetsCard
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.AppConfigToggleCard
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.ConfigDetailGroup
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.ConfigDetailPageBody
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.ConfigPageOverflowAction
import io.github.xiaotong6666.uihelper.adaptive.InfoBanner

@Composable
fun GlobalConfigPage(
    state: ConfigUiState,
    callbacks: ConfigCallbacks,
    onBack: () -> Unit,
    onSave: () -> Unit,
) {
    val view = LocalView.current
    val initialDrafts = globalConfigDrafts(state.currentHideConfig)
    var visibleExemptionsDraft by rememberSaveable { mutableStateOf(initialDrafts.visibleExemptionsText) }
    var hiddenTargetsDraft by rememberSaveable { mutableStateOf(initialDrafts.hiddenTargetsText) }
    var hiddenPackagesDraft by rememberSaveable { mutableStateOf(initialDrafts.hiddenPackagesText) }

    AppConfigPageScaffold(
        title = stringResource(R.string.global_hide_config_title),
        onBack = onBack,
        onSave = onSave,
        overflowActions = listOf(
            ConfigPageOverflowAction(
                label = stringResource(R.string.button_restore_defaults),
                onClick = {
                    val defaultDrafts = defaultGlobalConfigDrafts()
                    visibleExemptionsDraft = defaultDrafts.visibleExemptionsText
                    hiddenTargetsDraft = defaultDrafts.hiddenTargetsText
                    hiddenPackagesDraft = defaultDrafts.hiddenPackagesText
                    callbacks.onResetConfigClick()
                },
            ),
        ),
    ) { contentPadding, scrollModifier ->
        ConfigDetailPageBody(
            contentPadding = contentPadding,
            scrollModifier = scrollModifier,
        ) {
            InfoBanner(
                message = stringResource(R.string.section_editable_draft_desc),
            )

            ConfigDetailGroup {
                item {
                    AppConfigToggleCard(
                        checked = state.currentHideConfig.enableHideAllRootEntries,
                        title = stringResource(R.string.field_hide_all_title),
                        description = stringResource(R.string.field_hide_all_desc),
                        onToggle = {
                            view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                            callbacks.onConfigUpdate(
                                state.currentHideConfig.copy(enableHideAllRootEntries = !state.currentHideConfig.enableHideAllRootEntries),
                            )
                        },
                    )
                }
            }

            ConfigDetailGroup {
                item {
                    AppConfigTargetsCard(
                        value = visibleExemptionsDraft,
                        onValueChange = { newValue ->
                            visibleExemptionsDraft = newValue
                            callbacks.onConfigUpdate(updatedConfigForVisibleExemptions(state.currentHideConfig, newValue))
                        },
                        label = stringResource(R.string.field_visible_exemptions),
                        description = stringResource(R.string.field_visible_exemptions_help),
                        minLines = 5,
                        maxLines = 5,
                    )
                }
                item {
                    AppConfigTargetsCard(
                        value = hiddenTargetsDraft,
                        onValueChange = { newValue ->
                            hiddenTargetsDraft = newValue
                            callbacks.onConfigUpdate(updatedConfigForHiddenTargets(state.currentHideConfig, newValue))
                        },
                        label = stringResource(R.string.field_hidden_targets),
                        description = stringResource(R.string.field_hidden_targets_help),
                        minLines = 5,
                        maxLines = 5,
                    )
                }
                item {
                    AppConfigTargetsCard(
                        value = hiddenPackagesDraft,
                        onValueChange = { newValue ->
                            hiddenPackagesDraft = newValue
                            callbacks.onConfigUpdate(updatedConfigForHiddenPackages(state.currentHideConfig, newValue))
                        },
                        label = stringResource(R.string.field_hidden_package_names),
                        description = stringResource(R.string.field_hidden_package_names_help),
                        minLines = 5,
                        maxLines = 5,
                    )
                }
            }
        }
    }
}
