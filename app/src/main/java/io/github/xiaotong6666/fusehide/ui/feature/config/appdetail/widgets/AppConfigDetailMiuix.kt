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
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.outlined.ArrowBack
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.xiaotong6666.uihelper.common.StatusTag
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.DropdownImpl
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.ListPopupColumn
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.basic.PopupPositionProvider
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.Switch
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TopAppBar
import top.yukonga.miuix.kmp.overlay.OverlayListPopup
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic

@Composable
fun AppConfigPageScaffoldMiuix(
    title: String,
    onBack: () -> Unit,
    onSave: () -> Unit,
    overflowActions: List<ConfigPageOverflowAction>,
    content: @Composable (PaddingValues, Modifier) -> Unit,
) {
    val scrollBehavior = MiuixScrollBehavior()
    var showOverflowMenu by remember { mutableStateOf(false) }
    Scaffold(
        topBar = {
            TopAppBar(
                title = title,
                color = MiuixTheme.colorScheme.surface,
                titleColor = MiuixTheme.colorScheme.onSurface,
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        androidx.compose.material3.Icon(
                            imageVector = Icons.AutoMirrored.Outlined.ArrowBack,
                            contentDescription = null,
                            tint = MiuixTheme.colorScheme.onSurface,
                        )
                    }
                },
                actions = {
                    if (overflowActions.isNotEmpty()) {
                        IconButton(
                            onClick = { showOverflowMenu = true },
                            holdDownState = showOverflowMenu,
                        ) {
                            androidx.compose.material3.Icon(
                                imageVector = Icons.Default.MoreVert,
                                tint = MiuixTheme.colorScheme.onSurface,
                                contentDescription = null,
                            )
                        }
                        OverlayListPopup(
                            show = showOverflowMenu,
                            alignment = PopupPositionProvider.Align.TopEnd,
                            onDismissRequest = { showOverflowMenu = false },
                            content = {
                                ListPopupColumn {
                                    overflowActions.forEachIndexed { index, action ->
                                        DropdownImpl(
                                            text = action.label,
                                            optionSize = overflowActions.size,
                                            isSelected = false,
                                            index = index,
                                            onSelectedIndexChange = {
                                                showOverflowMenu = false
                                                action.onClick()
                                            },
                                        )
                                    }
                                }
                            },
                        )
                    }
                    IconButton(onClick = onSave) {
                        androidx.compose.material3.Icon(
                            imageVector = Icons.Default.Check,
                            contentDescription = null,
                            tint = MiuixTheme.colorScheme.onSurface,
                        )
                    }
                },
                scrollBehavior = scrollBehavior,
            )
        },
    ) { paddingValues ->
        content(paddingValues, Modifier.nestedScroll(scrollBehavior.nestedScrollConnection))
    }
}

@Composable
fun ConfigDetailPageBodyMiuix(
    contentPadding: PaddingValues,
    scrollModifier: Modifier,
    content: @Composable () -> Unit,
) {
    LazyColumn(
        modifier = Modifier
            .fillMaxWidth()
            .then(scrollModifier)
            .scrollEndHaptic()
            .overScrollVertical(),
        contentPadding = PaddingValues(
            start = 16.dp,
            end = 16.dp,
            top = contentPadding.calculateTopPadding() + 12.dp,
            bottom = contentPadding.calculateBottomPadding() + 20.dp,
        ),
        verticalArrangement = Arrangement.spacedBy(12.dp),
        overscrollEffect = null,
    ) {
        item {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                content()
                androidx.compose.foundation.layout.Spacer(modifier = Modifier.height(8.dp))
            }
        }
    }
}

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
        colors = CardDefaults.defaultColors(
            color = MiuixTheme.colorScheme.surfaceContainerHighest,
            contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest,
        ),
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
        colors = CardDefaults.defaultColors(
            color = MiuixTheme.colorScheme.surfaceContainerHighest,
            contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest,
        ),
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
        colors = CardDefaults.defaultColors(
            color = MiuixTheme.colorScheme.surfaceContainerHighest,
            contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest,
        ),
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
