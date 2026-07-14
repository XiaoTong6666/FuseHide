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

package io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.widgets

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.uihelper.adaptive.SectionDescription
import io.github.xiaotong6666.uihelper.adaptive.SectionTitle
import io.github.xiaotong6666.uihelper.chrome.DetailSettingsBody
import io.github.xiaotong6666.uihelper.chrome.DetailSettingsHost
import io.github.xiaotong6666.uihelper.chrome.DetailSettingsOverflowAction
import io.github.xiaotong6666.uihelper.material.primitive.SegmentedColumn
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import io.github.xiaotong6666.uihelper.model.SectionDescriptionStyle
import io.github.xiaotong6666.uihelper.model.SectionTitleStyle

typealias ConfigPageOverflowAction = DetailSettingsOverflowAction

class ConfigDetailGroupScope {
    internal val items = mutableListOf<@Composable () -> Unit>()

    fun item(content: @Composable () -> Unit) {
        items += content
    }
}

@Composable
fun AppConfigPageScaffold(
    title: String,
    onBack: () -> Unit,
    onSave: () -> Unit,
    overflowActions: List<ConfigPageOverflowAction> = emptyList(),
    content: @Composable (PaddingValues, Modifier) -> Unit,
) {
    DetailSettingsHost(title, onBack, onSave, overflowActions, content)
}

@Composable
fun ConfigDetailPageBody(
    contentPadding: PaddingValues,
    scrollModifier: Modifier,
    content: @Composable () -> Unit,
) {
    DetailSettingsBody(contentPadding, scrollModifier, content)
}

@Composable
fun ConfigDetailGroup(
    title: String = "",
    description: String? = null,
    content: ConfigDetailGroupScope.() -> Unit,
) {
    val items = ConfigDetailGroupScope().apply(content).items
    if (items.isEmpty()) return

    when (LocalUiMode.current) {
        UiMode.Material -> {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                if (title.isNotBlank() || !description.isNullOrBlank()) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        if (title.isNotBlank()) {
                            SectionTitle(text = title, style = SectionTitleStyle.Label)
                        }
                        if (!description.isNullOrBlank()) {
                            SectionDescription(text = description, style = SectionDescriptionStyle.Supporting)
                        }
                    }
                }
                SegmentedColumn {
                    items.forEach { entry ->
                        item { entry() }
                    }
                }
            }
        }

        UiMode.Miuix -> {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                if (title.isNotBlank() || !description.isNullOrBlank()) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        if (title.isNotBlank()) {
                            SectionTitle(text = title, style = SectionTitleStyle.Label)
                        }
                        if (!description.isNullOrBlank()) {
                            SectionDescription(text = description, style = SectionDescriptionStyle.Supporting)
                        }
                    }
                }
                items.forEach { entry ->
                    entry()
                }
            }
        }
    }
}

@Composable
fun AppConfigInfoCard(
    label: String,
    packageName: String,
    versionName: String,
    versionCode: Long,
    uid: Int,
    modifier: Modifier = Modifier,
    appIcon: (@Composable () -> Unit)? = null,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> AppConfigInfoCardMiuix(label, packageName, versionName, versionCode, uid, modifier, appIcon)
        UiMode.Material -> AppConfigInfoCardMaterial(label, packageName, versionName, versionCode, uid, modifier, appIcon)
    }
}

@Composable
fun AppConfigToggleCard(
    checked: Boolean,
    title: String,
    description: String,
    modifier: Modifier = Modifier,
    onToggle: () -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> AppConfigToggleCardMiuix(checked, title, description, modifier, onToggle)
        UiMode.Material -> AppConfigToggleCardMaterial(checked, title, description, modifier, onToggle)
    }
}

@Composable
fun AppConfigTargetsCard(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    description: String,
    modifier: Modifier = Modifier,
    minLines: Int = 5,
    maxLines: Int = 8,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> AppConfigTargetsCardMiuix(value, onValueChange, label, description, modifier, minLines, maxLines)
        UiMode.Material -> AppConfigTargetsCardMaterial(value, onValueChange, label, description, modifier, minLines, maxLines)
    }
}
