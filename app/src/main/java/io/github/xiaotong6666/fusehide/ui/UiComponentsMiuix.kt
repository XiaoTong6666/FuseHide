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
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.state.ToggleableState
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import top.yukonga.miuix.kmp.basic.Button
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Checkbox
import top.yukonga.miuix.kmp.basic.SmallTitle
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.basic.TextFieldDefaults
import top.yukonga.miuix.kmp.theme.MiuixTheme
import java.util.Locale

@Composable
fun SectionTitleMiuix(
    text: String,
    style: SectionTitleStyle,
) {
    Text(
        text = text,
        style = when (style) {
            SectionTitleStyle.Large -> MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium)
            SectionTitleStyle.Medium -> MiuixTheme.textStyles.title4
            SectionTitleStyle.EmphasizedMedium -> MiuixTheme.textStyles.title4.copy(fontWeight = FontWeight.Medium)
            SectionTitleStyle.Small -> MiuixTheme.textStyles.title4
            SectionTitleStyle.Label -> MiuixTheme.textStyles.footnote1.copy(fontWeight = FontWeight.Medium)
            SectionTitleStyle.Subsection -> MiuixTheme.textStyles.title4.copy(fontWeight = FontWeight.Medium)
        },
        color = if (style == SectionTitleStyle.Label) MiuixTheme.colorScheme.onSurfaceVariantSummary else MiuixTheme.colorScheme.onSurface,
    )
}

@Composable
fun SectionDescriptionMiuix(
    text: String,
    style: SectionDescriptionStyle,
) {
    Text(
        text = text,
        style = when (style) {
            SectionDescriptionStyle.Body -> MiuixTheme.textStyles.body1
            SectionDescriptionStyle.Supporting -> MiuixTheme.textStyles.footnote1
        },
        color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
    )
}

@Composable
fun AppTextFieldMiuix(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    singleLine: Boolean = false,
) {
    TextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier,
        label = label,
        colors = TextFieldDefaults.textFieldColors(
            backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
            labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            borderColor = MiuixTheme.colorScheme.primary,
        ),
        textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurface),
        singleLine = singleLine,
    )
}

@Composable
fun ConfigTextFieldMiuix(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    minLines: Int = 1,
    maxLines: Int = Int.MAX_VALUE,
) {
    TextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier,
        label = label,
        colors = TextFieldDefaults.textFieldColors(
            backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
            labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            borderColor = MiuixTheme.colorScheme.primary,
        ),
        textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurfaceSecondary),
        minLines = minLines,
        maxLines = maxLines,
    )
}

@Composable
fun ConfigToggleCardMiuix(
    checked: Boolean,
    title: String,
    description: String,
    onToggle: () -> Unit,
) {
    Card(
        onClick = onToggle,
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
            verticalAlignment = Alignment.Top,
        ) {
            Checkbox(
                state = if (checked) ToggleableState.On else ToggleableState.Off,
                onClick = null,
            )
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(title, style = MiuixTheme.textStyles.headline2)
                Text(
                    text = description,
                    style = MiuixTheme.textStyles.footnote1,
                    color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                )
            }
        }
    }
}

@Composable
fun InlineTextButtonMiuix(
    label: String,
    onClick: () -> Unit,
) {
    TextButton(
        text = label,
        onClick = onClick,
    )
}

@Composable
fun DualActionRowMiuix(
    primaryLabel: String,
    onPrimaryClick: () -> Unit,
    secondaryLabel: String,
    onSecondaryClick: () -> Unit,
    primaryFilled: Boolean = true,
) {
    val hasPrimary = primaryLabel.isNotEmpty()
    val hasSecondary = secondaryLabel.isNotEmpty()
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = if (hasPrimary && hasSecondary) Arrangement.spacedBy(10.dp) else Arrangement.Start,
    ) {
        val weight = if (hasPrimary && hasSecondary) Modifier.weight(1f) else Modifier.fillMaxWidth()
        if (hasPrimary) {
            if (primaryFilled) {
                Button(
                    onClick = onPrimaryClick,
                    modifier = weight.height(56.dp),
                    colors = ButtonDefaults.buttonColorsPrimary(),
                ) {
                    Text(primaryLabel, maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium))
                }
            } else {
                Button(
                    onClick = onPrimaryClick,
                    modifier = weight.height(56.dp),
                    colors = ButtonDefaults.buttonColors().copy(color = MiuixTheme.colorScheme.secondary),
                ) {
                    Text(primaryLabel, maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium))
                }
            }
        }
        if (hasSecondary) {
            Button(
                onClick = onSecondaryClick,
                modifier = weight.height(56.dp),
                colors = ButtonDefaults.buttonColors().copy(color = MiuixTheme.colorScheme.secondary),
            ) {
                Text(secondaryLabel, maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium))
            }
        }
    }
}

@Composable
fun PrimaryActionButtonMiuix(
    label: String,
    onClick: () -> Unit,
) {
    Button(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().height(56.dp),
        colors = ButtonDefaults.buttonColorsPrimary(),
    ) {
        Text(
            text = label,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            textAlign = TextAlign.Center,
            style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium),
        )
    }
}

@Composable
fun ActionGridMiuix(actions: List<GridActionItem>) {
    actions.chunked(2).forEach { rowActions ->
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            rowActions.forEach { item ->
                val currentBgColor = if (item.isError) MiuixTheme.colorScheme.error else MiuixTheme.colorScheme.secondary
                Button(
                    onClick = item.action,
                    modifier = Modifier.weight(1f).height(56.dp),
                    colors = ButtonDefaults.buttonColors().copy(color = currentBgColor),
                    insideMargin = PaddingValues(horizontal = 12.dp, vertical = 14.dp),
                ) {
                    Text(
                        text = item.label,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        textAlign = TextAlign.Center,
                        color = if (item.isError) MiuixTheme.colorScheme.onError else Color.Unspecified,
                        style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium),
                    )
                }
            }
            if (rowActions.size == 1) {
                Spacer(modifier = Modifier.weight(1f))
            }
        }
        Spacer(Modifier.height(10.dp))
    }
}

@Composable
fun SectionCardMiuix(content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        content = content,
    )
}

@Composable
fun SettingsGroupMiuix(content: @Composable ColumnScope.() -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.defaultColors(
            color = MiuixTheme.colorScheme.surfaceContainer.copy(alpha = 0.72f),
            contentColor = MiuixTheme.colorScheme.onSurfaceContainerHigh,
        ),
        insideMargin = PaddingValues(0.dp),
    ) {
        Column(modifier = Modifier.fillMaxWidth(), content = content)
    }
}

@Composable
fun SettingsGroupHeaderMiuix(text: String) {
    Box(modifier = Modifier.padding(bottom = 4.dp)) {
        SmallTitle(text)
    }
}

@Composable
fun SettingsToggleItemMiuix(
    checked: Boolean,
    title: String,
    description: String,
    onToggle: () -> Unit,
) {
    SettingsItemSurfaceMiuix(onClick = onToggle) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 14.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(2.dp),
            ) {
                Text(
                    text = title,
                    style = MiuixTheme.textStyles.headline2,
                    color = MiuixTheme.colorScheme.onSurface,
                )
                Text(
                    text = description,
                    style = MiuixTheme.textStyles.footnote1,
                    color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                )
            }
            Switch(
                checked = checked,
                onCheckedChange = null,
                modifier = Modifier.clearAndSetSemantics {},
                colors = SwitchDefaults.colors(
                    checkedThumbColor = MiuixTheme.colorScheme.onPrimary,
                    checkedTrackColor = MiuixTheme.colorScheme.primary,
                    checkedBorderColor = Color.Transparent,
                    checkedIconColor = MiuixTheme.colorScheme.primary,
                    uncheckedThumbColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                    uncheckedTrackColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                    uncheckedBorderColor = MiuixTheme.colorScheme.outline.copy(alpha = 0.5f),
                    uncheckedIconColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                ),
            )
        }
    }
}

@Composable
fun SettingsInfoItemMiuix(
    title: String,
    value: String,
) {
    SettingsItemSurfaceMiuix {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 14.dp),
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(
                    text = title,
                    style = MiuixTheme.textStyles.footnote2,
                    color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                )
                Text(
                    text = value,
                    style = MiuixTheme.textStyles.body1,
                    color = MiuixTheme.colorScheme.onSurface,
                )
            }
        }
    }
}

@Composable
fun SettingsGroupDividerMiuix() {
    Spacer(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp)
            .height(1.dp),
    )
}

@Composable
private fun SettingsItemSurfaceMiuix(
    onClick: (() -> Unit)? = null,
    content: @Composable () -> Unit,
) {
    if (onClick != null) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            onClick = onClick,
            colors = CardDefaults.defaultColors(
                color = Color.Transparent,
                contentColor = MiuixTheme.colorScheme.onSurface,
            ),
            insideMargin = PaddingValues(0.dp),
        ) {
            Column(modifier = Modifier.fillMaxWidth()) {
                content()
            }
        }
    } else {
        Box(modifier = Modifier.fillMaxWidth()) {
            content()
        }
    }
}

@Composable
fun StatusChipMiuix(
    label: String,
    value: String,
    modifier: Modifier = Modifier,
    supportingText: String? = null,
    metaText: String? = null,
    emphasized: Boolean = false,
    onClick: (() -> Unit)? = null,
) {
    val containerColor = if (emphasized) {
        MiuixTheme.colorScheme.primaryVariant
    } else {
        MiuixTheme.colorScheme.surfaceContainerHighest
    }
    val contentColor = if (emphasized) {
        MiuixTheme.colorScheme.onPrimaryVariant
    } else {
        MiuixTheme.colorScheme.onSurfaceContainerHighest
    }
    Card(
        modifier = modifier.heightIn(min = 118.dp),
        colors = CardDefaults.defaultColors(color = containerColor, contentColor = contentColor),
        onClick = onClick,
        insideMargin = PaddingValues(0.dp),
    ) {
        Column(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Text(
                text = label.uppercase(Locale.US),
                style = MiuixTheme.textStyles.footnote2,
                color = if (emphasized) {
                    MiuixTheme.colorScheme.onPrimaryVariant.copy(alpha = 0.72f)
                } else {
                    MiuixTheme.colorScheme.onSurfaceVariantSummary
                },
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = value,
                style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium),
                color = if (emphasized) Color.White else MiuixTheme.colorScheme.onSurfaceContainerHighest,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            if (supportingText != null) {
                Text(
                    text = supportingText,
                    style = MiuixTheme.textStyles.footnote1,
                    color = if (emphasized) {
                        MiuixTheme.colorScheme.onPrimaryVariant.copy(alpha = 0.84f)
                    } else {
                        MiuixTheme.colorScheme.onSurfaceVariantSummary
                    },
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            if (metaText != null) {
                Text(
                    text = metaText,
                    style = MiuixTheme.textStyles.footnote1,
                    color = if (emphasized) {
                        MiuixTheme.colorScheme.onPrimaryVariant.copy(alpha = 0.84f)
                    } else {
                        MiuixTheme.colorScheme.onSurfaceVariantSummary
                    },
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}

@Composable
fun MetricCardMiuix(
    label: String,
    value: String,
    modifier: Modifier = Modifier,
    valueMaxLines: Int = 2,
    monospace: Boolean = false,
) {
    Card(
        modifier = modifier.heightIn(min = 96.dp),
        colors = CardDefaults.defaultColors(
            color = MiuixTheme.colorScheme.surfaceContainerHighest,
            contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest,
        ),
        insideMargin = PaddingValues(0.dp),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Text(
                text = label.uppercase(Locale.US),
                style = MiuixTheme.textStyles.footnote2,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = value,
                style = MiuixTheme.textStyles.main,
                fontFamily = if (monospace) FontFamily.Monospace else FontFamily.Default,
                maxLines = valueMaxLines,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

@Composable
fun InfoPanelMiuix(
    title: String,
    text: String,
    monospace: Boolean = false,
    emphasized: Boolean = false,
) {
    val containerColor = if (emphasized) {
        MiuixTheme.colorScheme.errorContainer
    } else {
        MiuixTheme.colorScheme.surfaceContainerHighest
    }
    val contentColor = if (emphasized) {
        MiuixTheme.colorScheme.onErrorContainer
    } else {
        MiuixTheme.colorScheme.onSurfaceContainerHighest
    }
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.defaultColors(color = containerColor, contentColor = contentColor),
        insideMargin = PaddingValues(0.dp),
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            if (title.isNotEmpty()) {
                Text(title, style = MiuixTheme.textStyles.footnote2, color = MiuixTheme.colorScheme.onSurface.copy(alpha = 0.45f))
            }
            SelectionContainer {
                Text(
                    text = text,
                    modifier = Modifier
                        .fillMaxWidth()
                        .wrapContentHeight(),
                    fontFamily = if (monospace) FontFamily.Monospace else FontFamily.Default,
                    style = MiuixTheme.textStyles.body1,
                )
            }
        }
    }
}

@Composable
fun MonospaceBlockMiuix(text: String, modifier: Modifier = Modifier) {
    SelectionContainer {
        Text(
            text = text,
            modifier = modifier
                .fillMaxWidth()
                .wrapContentHeight(),
            fontFamily = FontFamily.Monospace,
            style = MiuixTheme.textStyles.body1,
        )
    }
}

@Composable
fun DeviceStatusListMiuix(infoPairs: List<Pair<String, String>>) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        insideMargin = PaddingValues(16.dp),
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            infoPairs.forEachIndexed { index, pair ->
                val isLast = index == infoPairs.lastIndex
                Text(
                    text = pair.first,
                    fontSize = MiuixTheme.textStyles.headline1.fontSize,
                    fontWeight = FontWeight.Medium,
                    color = MiuixTheme.colorScheme.onSurface,
                )
                Text(
                    text = pair.second,
                    fontSize = MiuixTheme.textStyles.body2.fontSize,
                    color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                    modifier = Modifier.padding(top = 2.dp, bottom = if (isLast) 0.dp else 24.dp),
                )
            }
        }
    }
}

@Composable
fun RuntimeSummaryCardMiuix(
    summaryText: String,
    snapshotText: String,
    emphasized: Boolean,
) {
    val contentColor = if (emphasized) {
        MiuixTheme.colorScheme.onErrorContainer
    } else {
        MiuixTheme.colorScheme.onSurface
    }
    val cardColors = if (emphasized) {
        CardDefaults.defaultColors(
            color = MiuixTheme.colorScheme.errorContainer,
            contentColor = contentColor,
        )
    } else {
        CardDefaults.defaultColors(contentColor = contentColor)
    }
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = cardColors,
        insideMargin = PaddingValues(0.dp),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
        ) {
            if (summaryText.isNotEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp, vertical = 14.dp),
                ) {
                    Text(
                        text = summaryText,
                        style = MiuixTheme.textStyles.body1,
                        color = contentColor,
                    )
                }
            }
            if (summaryText.isNotEmpty() && snapshotText.isNotEmpty()) {
                SettingsGroupDividerMiuix()
            }
            if (snapshotText.isNotEmpty()) {
                SelectionContainer {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(max = 200.dp)
                            .verticalScroll(rememberScrollState())
                            .padding(horizontal = 16.dp, vertical = 14.dp),
                    ) {
                        Text(
                            text = snapshotText,
                            fontFamily = FontFamily.Monospace,
                            style = MiuixTheme.textStyles.body1,
                            color = contentColor,
                        )
                    }
                }
            }
        }
    }
}
