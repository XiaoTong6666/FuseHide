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

package io.github.xiaotong6666.uihelper.material.primitive

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import java.util.Locale

@Composable
fun StatusChipMaterial(
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
    val containerColor = if (emphasized) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceBright
    val contentColor = if (emphasized) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurface
    TonalCardMaterial(
        modifier = modifier.heightIn(min = 124.dp),
        containerColor = containerColor,
        contentColor = contentColor,
        onClick = onClick,
    ) {
        StatusChipMaterialContent(
            label = label,
            value = value,
            supportingText = supportingText,
            metaText = metaText,
            supportingMinLines = supportingMinLines,
            metaMinLines = metaMinLines,
            emphasized = emphasized,
            contentColor = contentColor,
        )
    }
}

@Composable
private fun StatusChipMaterialContent(
    label: String,
    value: String,
    supportingText: String?,
    metaText: String?,
    supportingMinLines: Int,
    metaMinLines: Int,
    emphasized: Boolean,
    contentColor: Color,
) {
    Column(
        modifier = Modifier.padding(horizontal = 18.dp, vertical = 16.dp),
        verticalArrangement = Arrangement.spacedBy(5.dp),
    ) {
        Text(
            text = label.uppercase(Locale.US),
            style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.SemiBold),
            color = if (emphasized) contentColor.copy(alpha = 0.78f) else MaterialTheme.colorScheme.primary,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Text(
            text = value,
            style = MaterialTheme.typography.headlineSmall,
            color = contentColor,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        if (supportingText != null || supportingMinLines > 0) {
            val reservedSupportingLines = maxOf(1, supportingMinLines)
            Text(
                text = supportingText.orEmpty(),
                style = MaterialTheme.typography.bodyMedium,
                color = if (supportingText.isNullOrEmpty()) {
                    Color.Transparent
                } else if (emphasized) {
                    contentColor.copy(alpha = 0.86f)
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
                minLines = reservedSupportingLines,
                maxLines = maxOf(2, reservedSupportingLines),
                overflow = TextOverflow.Ellipsis,
            )
        }
        if (metaText != null || metaMinLines > 0) {
            val reservedMetaLines = maxOf(1, metaMinLines)
            Text(
                text = metaText.orEmpty(),
                style = MaterialTheme.typography.bodySmall,
                color = if (metaText.isNullOrEmpty()) {
                    Color.Transparent
                } else if (emphasized) {
                    contentColor.copy(alpha = 0.78f)
                } else {
                    MaterialTheme.colorScheme.onSurfaceVariant
                },
                minLines = reservedMetaLines,
                maxLines = maxOf(1, reservedMetaLines),
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
    TonalCardMaterial(
        modifier = modifier.heightIn(min = 100.dp),
        containerColor = MaterialTheme.colorScheme.surfaceBright,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 18.dp, vertical = 16.dp),
            verticalArrangement = Arrangement.spacedBy(5.dp),
        ) {
            Text(
                text = label.uppercase(Locale.US),
                style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.SemiBold),
                color = MaterialTheme.colorScheme.primary,
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
    val containerColor = if (emphasized) MaterialTheme.colorScheme.errorContainer else MaterialTheme.colorScheme.surfaceContainerHigh
    val contentColor = if (emphasized) MaterialTheme.colorScheme.onErrorContainer else MaterialTheme.colorScheme.onSurface
    TonalCardMaterial(
        modifier = Modifier.fillMaxWidth(),
        containerColor = containerColor,
        contentColor = contentColor,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(18.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            if (title.isNotEmpty()) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.labelLarge.copy(fontWeight = FontWeight.SemiBold),
                    color = contentColor.copy(alpha = 0.72f),
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
    TonalCardMaterial(
        modifier = Modifier.fillMaxWidth(),
        containerColor = MaterialTheme.colorScheme.surfaceBright,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(18.dp),
        ) {
            infoPairs.forEachIndexed { index, pair ->
                val isLast = index == infoPairs.lastIndex
                Text(
                    text = pair.first,
                    style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.SemiBold),
                    color = MaterialTheme.colorScheme.onSurface,
                )
                Text(
                    text = pair.second,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.padding(top = 4.dp, bottom = if (isLast) 0.dp else 18.dp),
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
    val containerColor = if (emphasized) MaterialTheme.colorScheme.errorContainer else MaterialTheme.colorScheme.surfaceBright
    val contentColor = if (emphasized) MaterialTheme.colorScheme.onErrorContainer else MaterialTheme.colorScheme.onSurface
    TonalCardMaterial(
        modifier = Modifier.fillMaxWidth(),
        containerColor = containerColor,
        contentColor = contentColor,
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            if (summaryText.isNotEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 18.dp, vertical = 16.dp),
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
                            .heightIn(max = 220.dp)
                            .verticalScroll(rememberScrollState())
                            .padding(horizontal = 18.dp, vertical = 16.dp),
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
