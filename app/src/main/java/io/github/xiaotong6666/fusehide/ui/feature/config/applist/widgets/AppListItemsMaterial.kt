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
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material3.Icon
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme.colorScheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.uihelper.common.StatusTag
import io.github.xiaotong6666.uihelper.extensions.androidapp.AppIconImage
import io.github.xiaotong6666.uihelper.material.primitive.SegmentedItem
import io.github.xiaotong6666.uihelper.material.primitive.SegmentedListItem

@Composable
fun AppListGroupMaterial(
    group: GroupedApps,
    hiddenPackages: Set<String>,
    enabledLabel: String,
    expanded: Boolean,
    onToggleExpand: () -> Unit,
    onOpenApp: (String) -> Unit,
    matchedPackageNames: Set<String> = emptySet(),
    alwaysShowChildren: Boolean = false,
    index: Int? = null,
    count: Int? = null,
) {
    val content: @Composable () -> Unit = {
        Column {
            GroupItemMaterial(
                group = group,
                hiddenPackages = hiddenPackages,
                enabledLabel = enabledLabel,
                selected = expanded,
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
                        SimpleAppItemMaterial(
                            app = app,
                            hiddenPackages = hiddenPackages,
                            enabledLabel = enabledLabel,
                            matched = matchedPackageNames.contains(app.packageName),
                        ) {
                            onOpenApp(app.packageName)
                        }
                    }
                }
            }
        }
    }

    if (index != null && count != null) {
        SegmentedItem(index = index, count = count, content = content)
    } else {
        content()
    }
}

@Composable
private fun SimpleAppItemMaterial(
    app: AppInfo,
    hiddenPackages: Set<String>,
    enabledLabel: String,
    matched: Boolean = false,
    onNavigate: () -> Unit,
) {
    Surface(
        modifier = Modifier.padding(horizontal = 4.dp),
        onClick = onNavigate,
        color = if (matched) colorScheme.secondaryContainer else colorScheme.surfaceBright,
        shape = androidx.compose.material3.MaterialTheme.shapes.large,
    ) {
        ListItem(
            colors = ListItemDefaults.colors(containerColor = Color.Transparent),
            content = { Text(app.label, overflow = TextOverflow.Ellipsis, maxLines = 1) },
            supportingContent = { Text(app.packageName, overflow = TextOverflow.Ellipsis, maxLines = 1) },
            leadingContent = {
                AppIconImage(
                    applicationInfo = app.applicationInfo,
                    label = app.label,
                    modifier = Modifier
                        .size(40.dp)
                        .padding(start = 4.dp),
                )
            },
            trailingContent = {
                if (app.packageName in hiddenPackages) {
                    StatusTag(
                        label = enabledLabel,
                        backgroundColor = colorScheme.primaryContainer,
                        contentColor = colorScheme.onPrimaryContainer,
                    )
                } else {
                    Icon(Icons.Filled.Remove, contentDescription = null, modifier = Modifier.padding(end = 4.dp))
                }
            },
        )
    }
}

@Composable
private fun GroupItemMaterial(
    group: GroupedApps,
    hiddenPackages: Set<String>,
    enabledLabel: String,
    selected: Boolean,
    onToggleExpand: () -> Unit,
    onClickPrimary: () -> Unit,
) {
    val userId = group.uid / 100000
    val summaryText = if (group.apps.size > 1) "${group.apps.size} apps" else group.primary.packageName

    SegmentedListItem(
        selected = selected,
        onClick = onClickPrimary,
        onLongClick = if (group.apps.size > 1) onToggleExpand else null,
        headlineContent = {
            Text(
                text = if (group.apps.size > 1) ownerNameForUid(group.uid) else group.primary.label,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
        },
        supportingContent = {
            Text(
                text = summaryText,
                color = colorScheme.onSurfaceVariant,
                overflow = TextOverflow.Ellipsis,
                maxLines = 1,
            )
        },
        trailingContent = {
            Column(
                horizontalAlignment = Alignment.End,
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                if (group.primary.packageName in hiddenPackages) {
                    StatusTag(
                        label = enabledLabel,
                        backgroundColor = colorScheme.primaryContainer,
                        contentColor = colorScheme.onPrimaryContainer,
                    )
                }
                if (userId != 0) {
                    StatusTag(
                        label = "USER $userId",
                        backgroundColor = colorScheme.tertiaryContainer,
                        contentColor = colorScheme.onTertiaryContainer,
                    )
                }
            }
        },
        leadingContent = {
            AppIconImage(
                applicationInfo = group.primary.applicationInfo,
                label = group.primary.label,
                modifier = Modifier.size(48.dp),
            )
        },
    )
}
