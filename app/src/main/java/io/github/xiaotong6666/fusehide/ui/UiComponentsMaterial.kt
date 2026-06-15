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
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
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
            SectionTitleStyle.Subsection -> MaterialTheme.typography.titleLarge
        },
        color = MaterialTheme.colorScheme.onSurface,
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
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.elevatedCardColors(),
        shape = MaterialTheme.shapes.medium,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
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
