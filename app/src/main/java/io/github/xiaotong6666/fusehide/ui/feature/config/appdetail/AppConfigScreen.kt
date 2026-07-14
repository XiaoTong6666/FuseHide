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

package io.github.xiaotong6666.fusehide.ui.feature.config.appdetail

import android.view.HapticFeedbackConstants
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.adapter.appSpecificTargetsText
import io.github.xiaotong6666.fusehide.ui.adapter.updatedConfigForPackageRule
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.AppConfigInfoCard
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.AppConfigPageScaffold
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.AppConfigTargetsCard
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.AppConfigToggleCard
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.ConfigDetailGroup
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets.ConfigDetailPageBody
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.AppListViewModel
import io.github.xiaotong6666.uihelper.extensions.androidapp.AppIconImage
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode

@Composable
fun AppConfigPage(
    packageName: String,
    state: ConfigUiState,
    callbacks: ConfigCallbacks,
    appListViewModel: AppListViewModel,
    onBack: () -> Unit,
    onSave: () -> Unit,
) {
    val view = LocalView.current
    val uiState by appListViewModel.uiState.collectAsState()
    val appInfo = uiState.groupedApps.flatMap { it.apps }.find { it.packageName == packageName }

    val isHiddenGlobally = state.currentHideConfig.hiddenPackages.contains(packageName)
    val specificRule = state.currentHideConfig.packageRules.find { it.packageName == packageName }
    val isHiddenSpecifically = specificRule != null
    val isEnabled = isHiddenGlobally || isHiddenSpecifically

    val specificTargetsText = appSpecificTargetsText(state.currentHideConfig, packageName)
    var specificTargetsDraft by rememberSaveable(packageName) { mutableStateOf(specificTargetsText) }
    val topBarTitle = appInfo?.label ?: packageName

    AppConfigPageScaffold(
        title = topBarTitle,
        onBack = onBack,
        onSave = onSave,
    ) { contentPadding, scrollModifier ->
        ConfigDetailPageBody(
            contentPadding = contentPadding,
            scrollModifier = scrollModifier.fillMaxSize(),
        ) {
            ConfigDetailGroup {
                item {
                    AppConfigInfoCard(
                        label = appInfo?.label ?: packageName,
                        packageName = appInfo?.packageName ?: packageName,
                        versionName = appInfo?.packageInfo?.versionName.orEmpty(),
                        versionCode = appInfo?.packageInfo?.longVersionCode ?: 0L,
                        uid = appInfo?.uid ?: -1,
                        appIcon = appInfo?.let {
                            {
                                AppIconImage(
                                    applicationInfo = it.applicationInfo,
                                    label = it.label,
                                    modifier = Modifier.size(if (LocalUiMode.current == UiMode.Miuix) 64.dp else 48.dp),
                                )
                            }
                        },
                    )
                }
            }
            ConfigDetailGroup {
                item {
                    AppConfigToggleCard(
                        checked = isEnabled,
                        title = stringResource(R.string.enable_hide_title),
                        description = stringResource(R.string.enable_hide_desc),
                        onToggle = {
                            view.performHapticFeedback(HapticFeedbackConstants.CLOCK_TICK)
                            callbacks.onConfigUpdate(
                                updatedConfigForPackageRule(
                                    currentConfig = state.currentHideConfig,
                                    packageName = packageName,
                                    enabled = !isEnabled,
                                    targetsText = specificTargetsDraft,
                                ),
                            )
                        },
                    )
                }
                item {
                    AppConfigTargetsCard(
                        value = specificTargetsDraft,
                        onValueChange = { newText ->
                            specificTargetsDraft = newText
                            callbacks.onConfigUpdate(
                                updatedConfigForPackageRule(
                                    currentConfig = state.currentHideConfig,
                                    packageName = packageName,
                                    enabled = isEnabled,
                                    targetsText = newText,
                                ),
                            )
                        },
                        label = stringResource(R.string.subdirectory_title),
                        description = stringResource(R.string.subdirectory_desc),
                    )
                }
            }
        }
    }
}
