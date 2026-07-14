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
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.xiaotong6666.uihelper.common.StatusTag
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Switch
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun AppConfigInfoCardMiuix(
    label: String,
    packageName: String,
    versionName: String,
    versionCode: Long,
    uid: Int,
    modifier: Modifier = Modifier,
    appIcon: (@Composable () -> Unit)? = null,
) {
    val userId = uid / 100000
    val appId = if (uid >= 0) uid % 100000 else -1
    Card(
        modifier = modifier.fillMaxWidth(),
        insideMargin = PaddingValues(start = 12.dp, end = 16.dp, top = 10.dp, bottom = 10.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            appIcon?.invoke()
            Column(
                modifier = Modifier
                    .padding(start = if (appIcon != null) 12.dp else 0.dp, end = 8.dp)
                    .weight(1f),
                verticalArrangement = Arrangement.spacedBy(2.dp),
            ) {
                Text(
                    text = label,
                    style = MiuixTheme.textStyles.headline2,
                    color = MiuixTheme.colorScheme.onSurface,
                    fontWeight = FontWeight(550),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                if (versionName.isNotBlank()) {
                    Text(
                        text = "$versionName ($versionCode)",
                        style = MiuixTheme.textStyles.footnote1,
                        color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                        fontWeight = FontWeight.Medium,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
                Text(
                    text = packageName,
                    style = MiuixTheme.textStyles.footnote1,
                    color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                    fontWeight = FontWeight.Medium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            if (uid >= 0) {
                Column(horizontalAlignment = Alignment.End, verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    if (userId != 0) {
                        StatusTag(
                            label = "USER $userId",
                            backgroundColor = MiuixTheme.colorScheme.primary.copy(alpha = 0.8f),
                            contentColor = MiuixTheme.colorScheme.onPrimary,
                        )
                        StatusTag(
                            label = "UID $appId",
                            backgroundColor = MiuixTheme.colorScheme.primary.copy(alpha = 0.8f),
                            contentColor = MiuixTheme.colorScheme.onPrimary,
                        )
                    } else {
                        StatusTag(
                            label = "UID $uid",
                            backgroundColor = MiuixTheme.colorScheme.primary.copy(alpha = 0.8f),
                            contentColor = MiuixTheme.colorScheme.onPrimary,
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun AppConfigToggleCardMiuix(
    checked: Boolean,
    title: String,
    description: String,
    modifier: Modifier = Modifier,
    onToggle: () -> Unit,
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        onClick = onToggle,
        insideMargin = PaddingValues(0.dp),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 14.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(text = title, style = MiuixTheme.textStyles.headline2, color = MiuixTheme.colorScheme.onSurface)
                Text(text = description, style = MiuixTheme.textStyles.footnote1, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
            }
            Switch(
                checked = checked,
                onCheckedChange = null,
            )
        }
    }
}

@Composable
fun AppConfigTargetsCardMiuix(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    description: String,
    modifier: Modifier = Modifier,
    minLines: Int = 5,
    maxLines: Int = 8,
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        insideMargin = PaddingValues(0.dp),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text(text = label, style = MiuixTheme.textStyles.headline2, color = MiuixTheme.colorScheme.onSurface)
            BasicTextField(
                value = value,
                onValueChange = onValueChange,
                modifier = Modifier.fillMaxWidth(),
                minLines = minLines,
                maxLines = maxLines,
                textStyle = TextStyle(
                    color = MiuixTheme.colorScheme.onSurface,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Medium,
                ),
                cursorBrush = SolidColor(MiuixTheme.colorScheme.primary),
                decorationBox = { innerTextField ->
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(min = (minLines * 24).dp)
                            .padding(vertical = 4.dp),
                    ) {
                        innerTextField()
                    }
                },
            )
            Text(text = description, style = MiuixTheme.textStyles.footnote1, color = MiuixTheme.colorScheme.onSurfaceVariantSummary)
        }
    }
}
