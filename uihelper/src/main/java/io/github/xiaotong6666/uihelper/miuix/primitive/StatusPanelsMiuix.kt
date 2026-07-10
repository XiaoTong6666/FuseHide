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

package io.github.xiaotong6666.uihelper.miuix.primitive

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.theme.MiuixTheme
import java.util.Locale

@Composable
fun StatusChipMiuix(
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
    val containerColor = if (emphasized) MiuixTheme.colorScheme.primaryVariant else MiuixTheme.colorScheme.surfaceContainerHighest
    val contentColor = if (emphasized) MiuixTheme.colorScheme.onPrimaryVariant else MiuixTheme.colorScheme.onSurfaceContainerHighest
    Card(modifier = modifier.heightIn(min = 118.dp), colors = CardDefaults.defaultColors(color = containerColor, contentColor = contentColor), onClick = onClick, insideMargin = PaddingValues(0.dp)) {
        Column(modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(text = label.uppercase(Locale.US), style = MiuixTheme.textStyles.footnote2, color = if (emphasized) MiuixTheme.colorScheme.onPrimaryVariant.copy(alpha = 0.72f) else MiuixTheme.colorScheme.onSurfaceVariantSummary, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(text = value, style = MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium), color = if (emphasized) Color.White else MiuixTheme.colorScheme.onSurfaceContainerHighest, maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (supportingText != null || supportingMinLines > 0) {
                val reservedSupportingLines = maxOf(1, supportingMinLines)
                Text(
                    text = supportingText.orEmpty(),
                    style = MiuixTheme.textStyles.footnote1,
                    color = if (supportingText.isNullOrEmpty()) {
                        Color.Transparent
                    } else if (emphasized) {
                        MiuixTheme.colorScheme.onPrimaryVariant.copy(alpha = 0.84f)
                    } else {
                        MiuixTheme.colorScheme.onSurfaceVariantSummary
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
                    style = MiuixTheme.textStyles.footnote1,
                    color = if (metaText.isNullOrEmpty()) {
                        Color.Transparent
                    } else if (emphasized) {
                        MiuixTheme.colorScheme.onPrimaryVariant.copy(alpha = 0.84f)
                    } else {
                        MiuixTheme.colorScheme.onSurfaceVariantSummary
                    },
                    minLines = reservedMetaLines,
                    maxLines = maxOf(1, reservedMetaLines),
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}

@Composable
fun MetricCardMiuix(label: String, value: String, modifier: Modifier = Modifier, valueMaxLines: Int = 2, monospace: Boolean = false) {
    Card(modifier = modifier.heightIn(min = 96.dp), colors = CardDefaults.defaultColors(color = MiuixTheme.colorScheme.surfaceContainerHighest, contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest), insideMargin = PaddingValues(0.dp)) {
        Column(modifier = Modifier.fillMaxWidth().padding(horizontal = 14.dp, vertical = 12.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(text = label.uppercase(Locale.US), style = MiuixTheme.textStyles.footnote2, color = MiuixTheme.colorScheme.onSurfaceVariantSummary, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(text = value, style = MiuixTheme.textStyles.main, fontFamily = if (monospace) FontFamily.Monospace else FontFamily.Default, maxLines = valueMaxLines, overflow = TextOverflow.Ellipsis)
        }
    }
}

@Composable
fun InfoPanelMiuix(title: String, text: String, monospace: Boolean = false, emphasized: Boolean = false) {
    val containerColor = if (emphasized) MiuixTheme.colorScheme.errorContainer else MiuixTheme.colorScheme.surfaceContainerHighest
    val contentColor = if (emphasized) MiuixTheme.colorScheme.onErrorContainer else MiuixTheme.colorScheme.onSurfaceContainerHighest
    Card(modifier = Modifier.fillMaxWidth(), colors = CardDefaults.defaultColors(color = containerColor, contentColor = contentColor), insideMargin = PaddingValues(0.dp)) {
        Column(modifier = Modifier.fillMaxWidth().padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            if (title.isNotEmpty()) Text(title, style = MiuixTheme.textStyles.footnote2, color = MiuixTheme.colorScheme.onSurface.copy(alpha = 0.45f))
            SelectionContainer { Text(text = text, modifier = Modifier.fillMaxWidth().wrapContentHeight(), fontFamily = if (monospace) FontFamily.Monospace else FontFamily.Default, style = MiuixTheme.textStyles.body1) }
        }
    }
}

@Composable
fun MonospaceBlockMiuix(text: String, modifier: Modifier = Modifier) {
    SelectionContainer { Text(text = text, modifier = modifier.fillMaxWidth().wrapContentHeight(), fontFamily = FontFamily.Monospace, style = MiuixTheme.textStyles.body1) }
}

@Composable
fun DeviceStatusListMiuix(infoPairs: List<Pair<String, String>>) {
    Card(modifier = Modifier.fillMaxWidth(), insideMargin = PaddingValues(16.dp)) {
        Column(modifier = Modifier.fillMaxWidth()) {
            infoPairs.forEachIndexed { index, pair ->
                val isLast = index == infoPairs.lastIndex
                Text(text = pair.first, fontSize = MiuixTheme.textStyles.headline1.fontSize, fontWeight = FontWeight.Medium, color = MiuixTheme.colorScheme.onSurface)
                Text(text = pair.second, fontSize = MiuixTheme.textStyles.body2.fontSize, color = MiuixTheme.colorScheme.onSurfaceVariantSummary, modifier = Modifier.padding(top = 2.dp, bottom = if (isLast) 0.dp else 24.dp))
            }
        }
    }
}

@Composable
fun RuntimeSummaryCardMiuix(summaryText: String, snapshotText: String, emphasized: Boolean) {
    val contentColor = if (emphasized) MiuixTheme.colorScheme.onErrorContainer else MiuixTheme.colorScheme.onSurface
    val cardColors = if (emphasized) CardDefaults.defaultColors(color = MiuixTheme.colorScheme.errorContainer, contentColor = contentColor) else CardDefaults.defaultColors(contentColor = contentColor)
    Card(modifier = Modifier.fillMaxWidth(), colors = cardColors, insideMargin = PaddingValues(0.dp)) {
        Column(modifier = Modifier.fillMaxWidth()) {
            if (summaryText.isNotEmpty()) Box(modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 14.dp)) { Text(text = summaryText, style = MiuixTheme.textStyles.body1, color = contentColor) }
            if (summaryText.isNotEmpty() && snapshotText.isNotEmpty()) SettingsGroupDividerMiuix()
            if (snapshotText.isNotEmpty()) {
                SelectionContainer {
                    Box(modifier = Modifier.fillMaxWidth().heightIn(max = 200.dp).verticalScroll(rememberScrollState()).padding(horizontal = 16.dp, vertical = 14.dp)) {
                        Text(text = snapshotText, fontFamily = FontFamily.Monospace, style = MiuixTheme.textStyles.body1, color = contentColor)
                    }
                }
            }
        }
    }
}
