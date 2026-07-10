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

package io.github.xiaotong6666.uihelper.adaptive

import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import io.github.xiaotong6666.uihelper.material.primitive.ActionGridMaterial
import io.github.xiaotong6666.uihelper.material.primitive.AppTextFieldMaterial
import io.github.xiaotong6666.uihelper.material.primitive.ConfigTextFieldMaterial
import io.github.xiaotong6666.uihelper.material.primitive.ConfigToggleCardMaterial
import io.github.xiaotong6666.uihelper.material.primitive.DeviceStatusListMaterial
import io.github.xiaotong6666.uihelper.material.primitive.DualActionRowMaterial
import io.github.xiaotong6666.uihelper.material.primitive.InfoBannerMaterial
import io.github.xiaotong6666.uihelper.material.primitive.InfoPanelMaterial
import io.github.xiaotong6666.uihelper.material.primitive.InlineTextButtonMaterial
import io.github.xiaotong6666.uihelper.material.primitive.MetricCardMaterial
import io.github.xiaotong6666.uihelper.material.primitive.MonospaceBlockMaterial
import io.github.xiaotong6666.uihelper.material.primitive.PrimaryActionButtonMaterial
import io.github.xiaotong6666.uihelper.material.primitive.RuntimeSummaryCardMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SectionCardMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SectionDescriptionMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SectionTitleMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SettingsGroupDividerMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SettingsGroupHeaderMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SettingsGroupMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SettingsInfoItemMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SettingsToggleItemMaterial
import io.github.xiaotong6666.uihelper.material.primitive.StatusChipMaterial
import io.github.xiaotong6666.uihelper.material.primitive.WarningBannerMaterial
import io.github.xiaotong6666.uihelper.miuix.primitive.ActionGridMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.AppTextFieldMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.ConfigTextFieldMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.ConfigToggleCardMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.DeviceStatusListMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.DualActionRowMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.InfoBannerMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.InfoPanelMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.InlineTextButtonMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.MetricCardMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.MonospaceBlockMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.PrimaryActionButtonMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.RuntimeSummaryCardMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SectionCardMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SectionDescriptionMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SectionTitleMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SettingsGroupDividerMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SettingsGroupHeaderMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SettingsGroupMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SettingsInfoItemMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.SettingsToggleItemMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.StatusChipMiuix
import io.github.xiaotong6666.uihelper.miuix.primitive.WarningBannerMiuix
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import io.github.xiaotong6666.uihelper.model.GridActionItem
import io.github.xiaotong6666.uihelper.model.SectionDescriptionStyle
import io.github.xiaotong6666.uihelper.model.SectionTitleStyle

@Composable
fun SectionTitle(
    text: String,
    style: SectionTitleStyle = SectionTitleStyle.Large,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SectionTitleMiuix(text, style)
        UiMode.Material -> SectionTitleMaterial(text, style)
    }
}

@Composable
fun SectionDescription(
    text: String,
    style: SectionDescriptionStyle = SectionDescriptionStyle.Body,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SectionDescriptionMiuix(text, style)
        UiMode.Material -> SectionDescriptionMaterial(text, style)
    }
}

@Composable
fun AppTextField(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    singleLine: Boolean = false,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> AppTextFieldMiuix(value, onValueChange, label, modifier, singleLine)
        UiMode.Material -> AppTextFieldMaterial(value, onValueChange, label, modifier, singleLine)
    }
}

@Composable
fun ConfigTextField(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    minLines: Int = 1,
    maxLines: Int = Int.MAX_VALUE,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ConfigTextFieldMiuix(value, onValueChange, label, modifier, minLines, maxLines)
        UiMode.Material -> ConfigTextFieldMaterial(value, onValueChange, label, modifier, minLines, maxLines)
    }
}

@Composable
fun ConfigToggleCard(
    checked: Boolean,
    title: String,
    description: String,
    onToggle: () -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ConfigToggleCardMiuix(checked, title, description, onToggle)
        UiMode.Material -> ConfigToggleCardMaterial(checked, title, description, onToggle)
    }
}

@Composable
fun InlineTextButton(
    label: String,
    onClick: () -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> InlineTextButtonMiuix(label, onClick)
        UiMode.Material -> InlineTextButtonMaterial(label, onClick)
    }
}

@Composable
fun DualActionRow(
    primaryLabel: String,
    onPrimaryClick: () -> Unit,
    secondaryLabel: String,
    onSecondaryClick: () -> Unit,
    primaryFilled: Boolean = true,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> DualActionRowMiuix(primaryLabel, onPrimaryClick, secondaryLabel, onSecondaryClick, primaryFilled)
        UiMode.Material -> DualActionRowMaterial(primaryLabel, onPrimaryClick, secondaryLabel, onSecondaryClick, primaryFilled)
    }
}

@Composable
fun PrimaryActionButton(
    label: String,
    onClick: () -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> PrimaryActionButtonMiuix(label, onClick)
        UiMode.Material -> PrimaryActionButtonMaterial(label, onClick)
    }
}

@Composable
fun SettingsGroup(content: @Composable ColumnScope.() -> Unit) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SettingsGroupMiuix(content)
        UiMode.Material -> SettingsGroupMaterial(content)
    }
}

@Composable
fun SettingsGroupHeader(text: String) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SettingsGroupHeaderMiuix(text)
        UiMode.Material -> SettingsGroupHeaderMaterial(text)
    }
}

@Composable
fun SettingsToggleItem(
    checked: Boolean,
    title: String,
    description: String,
    onToggle: () -> Unit,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SettingsToggleItemMiuix(checked, title, description, onToggle)
        UiMode.Material -> SettingsToggleItemMaterial(checked, title, description, onToggle)
    }
}

@Composable
fun SettingsInfoItem(
    title: String,
    value: String,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SettingsInfoItemMiuix(title, value)
        UiMode.Material -> SettingsInfoItemMaterial(title, value)
    }
}

@Composable
fun SettingsGroupDivider() {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SettingsGroupDividerMiuix()
        UiMode.Material -> SettingsGroupDividerMaterial()
    }
}

@Composable
fun ActionGrid(actions: List<GridActionItem>) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> ActionGridMiuix(actions)
        UiMode.Material -> ActionGridMaterial(actions)
    }
}

@Composable
fun SectionCard(content: @Composable ColumnScope.() -> Unit) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> SectionCardMiuix(content)
        UiMode.Material -> SectionCardMaterial(content)
    }
}

@Composable
fun WarningBanner(
    message: String,
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> WarningBannerMiuix(message, modifier, onClick)
        UiMode.Material -> WarningBannerMaterial(message, modifier, onClick)
    }
}

@Composable
fun InfoBanner(
    message: String,
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> InfoBannerMiuix(message, modifier, onClick)
        UiMode.Material -> InfoBannerMaterial(message, modifier, onClick)
    }
}

@Composable
fun StatusChip(
    label: String,
    value: String,
    modifier: Modifier = Modifier,
    supportingText: String? = null,
    metaText: String? = null,
    supportingMinLines: Int = 0,
    metaMinLines: Int = 0,
    emphasized: Boolean = false,
    onClick: (() -> Unit)? = null,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> StatusChipMiuix(label, value, modifier, supportingText, metaText, supportingMinLines, metaMinLines, emphasized, onClick)
        UiMode.Material -> StatusChipMaterial(label, value, modifier, supportingText, metaText, supportingMinLines, metaMinLines, emphasized, onClick)
    }
}

@Composable
fun MetricCard(
    label: String,
    value: String,
    modifier: Modifier = Modifier,
    valueMaxLines: Int = 2,
    monospace: Boolean = false,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> MetricCardMiuix(label, value, modifier, valueMaxLines, monospace)
        UiMode.Material -> MetricCardMaterial(label, value, modifier, valueMaxLines, monospace)
    }
}

@Composable
fun InfoPanel(
    title: String,
    text: String,
    monospace: Boolean = false,
    emphasized: Boolean = false,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> InfoPanelMiuix(title, text, monospace, emphasized)
        UiMode.Material -> InfoPanelMaterial(title, text, monospace, emphasized)
    }
}

@Composable
fun MonospaceBlock(text: String, modifier: Modifier = Modifier) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> MonospaceBlockMiuix(text, modifier)
        UiMode.Material -> MonospaceBlockMaterial(text, modifier)
    }
}

@Composable
fun DeviceStatusList(infoPairs: List<Pair<String, String>>) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> DeviceStatusListMiuix(infoPairs)
        UiMode.Material -> DeviceStatusListMaterial(infoPairs)
    }
}

@Composable
fun RuntimeSummaryCard(
    summaryText: String,
    snapshotText: String,
    emphasized: Boolean,
) {
    when (LocalUiMode.current) {
        UiMode.Miuix -> RuntimeSummaryCardMiuix(summaryText, snapshotText, emphasized)
        UiMode.Material -> RuntimeSummaryCardMaterial(summaryText, snapshotText, emphasized)
    }
}
