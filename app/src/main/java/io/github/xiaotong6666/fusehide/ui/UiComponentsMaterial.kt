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
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.util.Locale

@Composable
fun SectionTitleMaterial(
    text: String,
    style: SectionTitleStyle,
) {
    Text(
        text = text,
        style = when (style) {
            SectionTitleStyle.Large -> MaterialTheme.typography.titleLarge
            SectionTitleStyle.Medium -> MaterialTheme.typography.titleMedium
            SectionTitleStyle.EmphasizedMedium -> MaterialTheme.typography.titleMedium
            SectionTitleStyle.Small -> MaterialTheme.typography.titleSmall
            SectionTitleStyle.Label -> MaterialTheme.typography.titleSmall
            SectionTitleStyle.Subsection -> MaterialTheme.typography.titleLarge
        },
        color = if (style == SectionTitleStyle.Label) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
    )
}

@Composable
fun SectionDescriptionMaterial(
    text: String,
    style: SectionDescriptionStyle,
) {
    Text(
        text = text,
        style = when (style) {
            SectionDescriptionStyle.Body -> MaterialTheme.typography.bodyMedium
            SectionDescriptionStyle.Supporting -> MaterialTheme.typography.bodyMedium
        },
        color = MaterialTheme.colorScheme.onSurfaceVariant,
    )
}

@Composable
fun AppTextFieldMaterial(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    singleLine: Boolean = false,
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier,
        label = { Text(label) },
        colors = OutlinedTextFieldDefaults.colors(
            focusedBorderColor = MaterialTheme.colorScheme.primary,
            unfocusedBorderColor = MaterialTheme.colorScheme.outline,
            focusedLabelColor = MaterialTheme.colorScheme.primary,
            unfocusedLabelColor = MaterialTheme.colorScheme.onSurfaceVariant,
        ),
        textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onSurface),
        singleLine = singleLine,
    )
}

@Composable
fun ConfigTextFieldMaterial(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    minLines: Int = 1,
    maxLines: Int = Int.MAX_VALUE,
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier,
        label = { Text(label) },
        colors = OutlinedTextFieldDefaults.colors(
            focusedBorderColor = MaterialTheme.colorScheme.primary,
            unfocusedBorderColor = MaterialTheme.colorScheme.outline,
            focusedLabelColor = MaterialTheme.colorScheme.primary,
            unfocusedLabelColor = MaterialTheme.colorScheme.onSurfaceVariant,
        ),
        textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onSurface),
        minLines = minLines,
        maxLines = maxLines,
    )
}

@Composable
fun ConfigToggleCardMaterial(
    checked: Boolean,
    title: String,
    description: String,
    onToggle: () -> Unit,
) {
    ElevatedCard(
        onClick = onToggle,
        colors = CardDefaults.elevatedCardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
        ),
        shape = MaterialTheme.shapes.small,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(14.dp),
            verticalAlignment = Alignment.Top,
        ) {
            Checkbox(
                checked = checked,
                onCheckedChange = null,
            )
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(
                    title,
                    style = MaterialTheme.typography.titleMedium,
                )
                Text(
                    text = description,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
fun InlineTextButtonMaterial(
    label: String,
    onClick: () -> Unit,
) {
    TextButton(onClick = onClick) {
        Text(label)
    }
}

@Composable
fun DualActionRowMaterial(
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
        horizontalArrangement = if (hasPrimary && hasSecondary) Arrangement.spacedBy(12.dp) else Arrangement.Start,
    ) {
        val weight = if (hasPrimary && hasSecondary) Modifier.weight(1f) else Modifier.fillMaxWidth()
        if (hasPrimary) {
            if (primaryFilled) {
                Button(
                    onClick = onPrimaryClick,
                    modifier = weight.height(48.dp),
                    shape = MaterialTheme.shapes.small,
                ) {
                    Text(
                        primaryLabel,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        textAlign = TextAlign.Center,
                        style = MaterialTheme.typography.titleMedium,
                    )
                }
            } else {
                OutlinedButton(
                    onClick = onPrimaryClick,
                    modifier = weight.height(48.dp),
                    shape = MaterialTheme.shapes.small,
                ) {
                    Text(
                        primaryLabel,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        textAlign = TextAlign.Center,
                        style = MaterialTheme.typography.titleMedium,
                    )
                }
            }
        }
        if (hasSecondary) {
            OutlinedButton(
                onClick = onSecondaryClick,
                modifier = weight.height(48.dp),
                shape = MaterialTheme.shapes.small,
            ) {
                Text(
                    secondaryLabel,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    textAlign = TextAlign.Center,
                    style = MaterialTheme.typography.titleMedium,
                )
            }
        }
    }
}

@Composable
fun PrimaryActionButtonMaterial(
    label: String,
    onClick: () -> Unit,
) {
    Button(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().height(48.dp),
        shape = MaterialTheme.shapes.small,
    ) {
        Text(
            text = label,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            textAlign = TextAlign.Center,
            style = MaterialTheme.typography.titleMedium,
        )
    }
}

@Composable
fun SectionCardMaterial(content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        content = content,
    )
}

@Composable
fun SettingsGroupMaterial(content: @Composable ColumnScope.() -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        color = MaterialTheme.colorScheme.surfaceContainerLow,
        shape = MaterialTheme.shapes.medium,
    ) {
        Column(modifier = Modifier.fillMaxWidth(), content = content)
    }
}

@Composable
fun SettingsGroupHeaderMaterial(text: String) {
    Text(
        text = text,
        modifier = Modifier.padding(start = 32.dp, end = 16.dp, top = 8.dp, bottom = 12.dp),
        style = TextStyle(
            fontFamily = FontFamily.Default,
            fontWeight = FontWeight.Normal,
            fontSize = 14.sp,
            lineHeight = 20.sp,
            letterSpacing = 0.sp,
        ),
        color = MaterialTheme.colorScheme.primary,
    )
}

@Composable
fun SettingsToggleItemMaterial(
    checked: Boolean,
    title: String,
    description: String,
    onToggle: () -> Unit,
) {
    SettingsItemSurfaceMaterial(onClick = onToggle) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Column(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(2.dp),
            ) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    color = MaterialTheme.colorScheme.onSurface,
                )
                Text(
                    text = description,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Switch(
                checked = checked,
                onCheckedChange = null,
                modifier = Modifier.clearAndSetSemantics {},
                colors = SwitchDefaults.colors(
                    checkedThumbColor = MaterialTheme.colorScheme.onPrimary,
                    checkedTrackColor = MaterialTheme.colorScheme.primary,
                    checkedBorderColor = Color.Transparent,
                    checkedIconColor = MaterialTheme.colorScheme.primary,
                    uncheckedThumbColor = MaterialTheme.colorScheme.onSurfaceVariant,
                    uncheckedTrackColor = MaterialTheme.colorScheme.surfaceContainerHighest,
                    uncheckedBorderColor = MaterialTheme.colorScheme.outlineVariant,
                    uncheckedIconColor = MaterialTheme.colorScheme.surfaceContainerHighest,
                ),
            )
        }
    }
}

@Composable
fun SettingsInfoItemMaterial(
    title: String,
    value: String,
) {
    SettingsItemSurfaceMaterial {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 14.dp),
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Text(
                    text = value,
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onSurface,
                )
            }
        }
    }
}

@Composable
fun SettingsGroupDividerMaterial() {
    HorizontalDivider(
        modifier = Modifier.padding(horizontal = 16.dp),
        color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.55f),
        thickness = 0.75.dp,
    )
}

@Composable
private fun SettingsItemSurfaceMaterial(
    onClick: (() -> Unit)? = null,
    content: @Composable () -> Unit,
) {
    val shape = MaterialTheme.shapes.medium
    if (onClick != null) {
        Surface(
            modifier = Modifier
                .fillMaxWidth()
                .clip(shape),
            color = Color.Transparent,
            onClick = onClick,
            shape = shape,
            content = content,
        )
    } else {
        Surface(
            modifier = Modifier
                .fillMaxWidth()
                .clip(shape),
            color = Color.Transparent,
            shape = shape,
            content = content,
        )
    }
}

@Composable
fun StatusChipMaterial(
    label: String,
    value: String,
    modifier: Modifier = Modifier,
    supportingText: String? = null,
    metaText: String? = null,
    emphasized: Boolean = false,
    onClick: (() -> Unit)? = null,
) {
    val containerColor = if (emphasized) {
        MaterialTheme.colorScheme.primaryContainer
    } else {
        MaterialTheme.colorScheme.surfaceContainerHighest
    }
    val contentColor = if (emphasized) {
        MaterialTheme.colorScheme.onPrimaryContainer
    } else {
        MaterialTheme.colorScheme.onSurface
    }

    if (onClick != null) {
        ElevatedCard(
            modifier = modifier.heightIn(min = 118.dp),
            colors = CardDefaults.elevatedCardColors(
                containerColor = containerColor,
                contentColor = contentColor,
            ),
            onClick = onClick,
            shape = MaterialTheme.shapes.small,
        ) {
            StatusChipMaterialContent(label, value, supportingText, metaText, emphasized, contentColor)
        }
    } else {
        ElevatedCard(
            modifier = modifier.heightIn(min = 118.dp),
            colors = CardDefaults.elevatedCardColors(
                containerColor = containerColor,
                contentColor = contentColor,
            ),
            shape = MaterialTheme.shapes.small,
        ) {
            StatusChipMaterialContent(label, value, supportingText, metaText, emphasized, contentColor)
        }
    }
}

@Composable
private fun StatusChipMaterialContent(
    label: String,
    value: String,
    supportingText: String?,
    metaText: String?,
    emphasized: Boolean,
    contentColor: androidx.compose.ui.graphics.Color,
) {
    Column(
        modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        Text(
            text = label.uppercase(Locale.US),
            style = MaterialTheme.typography.labelSmall,
            color = if (emphasized) {
                contentColor.copy(alpha = 0.72f)
            } else {
                MaterialTheme.colorScheme.onSurfaceVariant
            },
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Text(
            text = value,
            style = MaterialTheme.typography.titleLarge,
            color = if (emphasized) contentColor else MaterialTheme.colorScheme.onSurface,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        if (supportingText != null) {
            Text(
                text = supportingText,
                style = MaterialTheme.typography.bodySmall,
                color = if (emphasized) {
                    contentColor.copy(alpha = 0.84f)
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
        }
        if (metaText != null) {
            Text(
                text = metaText,
                style = MaterialTheme.typography.bodySmall,
                color = if (emphasized) {
                    contentColor.copy(alpha = 0.84f)
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

@Composable
fun MetricCardMaterial(
    label: String,
    value: String,
    modifier: Modifier = Modifier,
    valueMaxLines: Int = 2,
    monospace: Boolean = false,
) {
    val containerColor = MaterialTheme.colorScheme.surfaceContainerHighest
    val contentColor = MaterialTheme.colorScheme.onSurface

    ElevatedCard(
        modifier = modifier.heightIn(min = 96.dp),
        colors = CardDefaults.elevatedCardColors(
            containerColor = containerColor,
            contentColor = contentColor,
        ),
        shape = MaterialTheme.shapes.small,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            Text(
                text = label.uppercase(Locale.US),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = value,
                style = MaterialTheme.typography.bodyLarge,
                fontFamily = if (monospace) FontFamily.Monospace else FontFamily.Default,
                maxLines = valueMaxLines,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

@Composable
fun InfoPanelMaterial(
    title: String,
    text: String,
    monospace: Boolean = false,
    emphasized: Boolean = false,
) {
    val containerColor = if (emphasized) {
        MaterialTheme.colorScheme.errorContainer
    } else {
        MaterialTheme.colorScheme.surfaceContainerHighest
    }
    val contentColor = if (emphasized) {
        MaterialTheme.colorScheme.onErrorContainer
    } else {
        MaterialTheme.colorScheme.onSurface
    }
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.elevatedCardColors(
            containerColor = containerColor,
            contentColor = contentColor,
        ),
        shape = MaterialTheme.shapes.small,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            if (title.isNotEmpty()) {
                Text(
                    title,
                    style = MaterialTheme.typography.labelSmall,
                    color = contentColor.copy(alpha = 0.6f),
                )
            }
            SelectionContainer {
                Text(
                    text = text,
                    modifier = Modifier
                        .fillMaxWidth()
                        .wrapContentHeight(),
                    fontFamily = if (monospace) FontFamily.Monospace else FontFamily.Default,
                    style = MaterialTheme.typography.bodyMedium,
                )
            }
        }
    }
}

@Composable
fun ActionGridMaterial(actions: List<GridActionItem>) {
    val rows = actions.chunked(2)
    rows.forEachIndexed { rowIndex, rowActions ->
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            rowActions.forEach { item ->
                val modifier = Modifier.weight(1f)
                val content: @Composable () -> Unit = {
                    Text(
                        item.label,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        textAlign = TextAlign.Center,
                        style = MaterialTheme.typography.titleMedium,
                    )
                }
                when {
                    item.isError -> OutlinedButton(
                        onClick = item.action,
                        modifier = modifier,
                        colors = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.error),
                        shape = MaterialTheme.shapes.small,
                        content = { content() },
                    )

                    item.style == GridActionStyle.Filled -> Button(
                        onClick = item.action,
                        modifier = modifier,
                        shape = MaterialTheme.shapes.small,
                        content = { content() },
                    )

                    item.style == GridActionStyle.Tonal -> FilledTonalButton(
                        onClick = item.action,
                        modifier = modifier,
                        shape = MaterialTheme.shapes.small,
                        content = { content() },
                    )

                    else -> OutlinedButton(
                        onClick = item.action,
                        modifier = modifier,
                        shape = MaterialTheme.shapes.small,
                        content = { content() },
                    )
                }
            }
            if (rowActions.size == 1) {
                Spacer(modifier = Modifier.weight(1f))
            }
        }
        if (rowIndex < rows.lastIndex) {
            Spacer(Modifier.height(8.dp))
        }
    }
}

@Composable
fun MonospaceBlockMaterial(text: String, modifier: Modifier = Modifier) {
    SelectionContainer {
        Text(
            text = text,
            modifier = modifier
                .fillMaxWidth()
                .wrapContentHeight(),
            fontFamily = FontFamily.Monospace,
            style = MaterialTheme.typography.bodyMedium,
        )
    }
}

@Composable
fun DeviceStatusListMaterial(infoPairs: List<Pair<String, String>>) {
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.elevatedCardColors(
            containerColor = MaterialTheme.colorScheme.surfaceContainerHighest,
            contentColor = MaterialTheme.colorScheme.onSurface,
        ),
        shape = MaterialTheme.shapes.small,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
        ) {
            infoPairs.forEachIndexed { index, pair ->
                val isLast = index == infoPairs.lastIndex
                Text(
                    text = pair.first,
                    style = MaterialTheme.typography.titleLarge,
                    color = MaterialTheme.colorScheme.onSurface,
                )
                Text(
                    text = pair.second,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(top = 2.dp, bottom = if (isLast) 0.dp else 24.dp),
                )
            }
        }
    }
}

@Composable
fun RuntimeSummaryCardMaterial(
    summaryText: String,
    snapshotText: String,
    emphasized: Boolean,
) {
    val containerColor = if (emphasized) {
        MaterialTheme.colorScheme.errorContainer
    } else {
        MaterialTheme.colorScheme.surfaceContainerHighest
    }
    val contentColor = if (emphasized) {
        MaterialTheme.colorScheme.onErrorContainer
    } else {
        MaterialTheme.colorScheme.onSurface
    }
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.elevatedCardColors(
            containerColor = containerColor,
            contentColor = contentColor,
        ),
        shape = MaterialTheme.shapes.small,
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
                        style = MaterialTheme.typography.bodyMedium,
                        color = contentColor,
                    )
                }
            }
            if (summaryText.isNotEmpty() && snapshotText.isNotEmpty()) {
                SettingsGroupDividerMaterial()
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
                            style = MaterialTheme.typography.bodyMedium,
                            color = contentColor,
                        )
                    }
                }
            }
        }
    }
}
