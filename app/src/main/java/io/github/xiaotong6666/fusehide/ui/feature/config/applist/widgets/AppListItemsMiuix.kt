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

package io.github.xiaotong6666.fusehide.ui.feature.config.applist.widgets

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.basicMarquee
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.xiaotong6666.uihelper.common.StatusTag
import io.github.xiaotong6666.uihelper.extensions.androidapp.AppIconImage
import top.yukonga.miuix.kmp.basic.BasicComponent
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.theme.MiuixTheme.colorScheme

@Composable
fun AppListGroupMiuix(
    group: GroupedApps,
    hiddenPackages: Set<String>,
    enabledLabel: String,
    expanded: Boolean,
    onToggleExpand: () -> Unit,
    onOpenApp: (String) -> Unit,
    matchedPackageNames: Set<String> = emptySet(),
    alwaysShowChildren: Boolean = false,
) {
    Column {
        GroupItemMiuix(
            group = group,
            hiddenPackages = hiddenPackages,
            enabledLabel = enabledLabel,
            onToggleExpand = onToggleExpand,
        ) {
            onOpenApp(group.primary.packageName)
        }
        AnimatedVisibility(
            visible = (expanded || alwaysShowChildren) && group.apps.size > 1,
            enter = expandVertically() + fadeIn(),
            exit = shrinkVertically() + fadeOut(),
        ) {
            Column {
                group.apps.forEach { app ->
                    SimpleAppItemMiuix(
                        app = app,
                        hiddenPackages = hiddenPackages,
                        enabledLabel = enabledLabel,
                        matched = matchedPackageNames.contains(app.packageName),
                    ) {
                        onOpenApp(app.packageName)
                    }
                }
                Spacer(Modifier.height(6.dp))
            }
        }
    }
}

@Composable
private fun SimpleAppItemMiuix(
    app: AppInfo,
    hiddenPackages: Set<String>,
    enabledLabel: String,
    matched: Boolean = false,
    onClick: () -> Unit,
) {
    Card(
        modifier = Modifier.padding(horizontal = 12.dp, vertical = 0.dp).padding(bottom = 6.dp),
        onClick = onClick,
    ) {
        BasicComponent(
            title = app.label,
            summary = app.packageName,
            startAction = {
                AppIconImage(
                    applicationInfo = app.applicationInfo,
                    label = app.label,
                    modifier = Modifier
                        .padding(end = 2.dp)
                        .size(40.dp),
                )
            },
            endActions = {
                if (app.packageName in hiddenPackages) {
                    StatusTag(
                        label = enabledLabel,
                        backgroundColor = colorScheme.primary,
                        contentColor = colorScheme.onPrimary,
                    )
                }
            },
            insideMargin = PaddingValues(horizontal = 9.dp),
        )
    }
}

@Composable
private fun GroupItemMiuix(
    group: GroupedApps,
    hiddenPackages: Set<String>,
    enabledLabel: String,
    onToggleExpand: () -> Unit,
    onClickPrimary: () -> Unit,
) {
    val userId = group.uid / 100000
    Card(
        modifier = Modifier
            .padding(horizontal = 12.dp)
            .padding(bottom = 12.dp),
        onClick = onClickPrimary,
        onLongPress = if (group.apps.size > 1) onToggleExpand else null,
        showIndication = true,
        insideMargin = PaddingValues(start = 10.dp, end = 16.dp, top = 8.dp, bottom = 8.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            AppIconImage(
                applicationInfo = group.primary.applicationInfo,
                label = group.primary.label,
                modifier = Modifier
                    .padding(end = 10.dp)
                    .size(48.dp),
            )
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = if (group.apps.size > 1) ownerNameForUid(group.uid) else group.primary.label,
                    modifier = Modifier.basicMarquee(),
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurface,
                    maxLines = 1,
                    softWrap = false,
                )
                Text(
                    text = if (group.apps.size > 1) "${group.apps.size} apps" else group.primary.packageName,
                    modifier = Modifier.basicMarquee(),
                    fontSize = 12.sp,
                    fontWeight = FontWeight(550),
                    color = colorScheme.onSurfaceVariantSummary,
                    maxLines = 1,
                    softWrap = false,
                )
            }
            Column(
                modifier = Modifier.padding(start = 16.dp),
                horizontalAlignment = Alignment.End,
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                if (group.primary.packageName in hiddenPackages) {
                    StatusTag(
                        label = enabledLabel,
                        backgroundColor = colorScheme.primary,
                        contentColor = colorScheme.onPrimary,
                    )
                }
                if (userId != 0) {
                    StatusTag(
                        label = "USER $userId",
                        backgroundColor = colorScheme.secondary,
                        contentColor = colorScheme.onSecondary,
                    )
                }
            }
        }
    }
}
