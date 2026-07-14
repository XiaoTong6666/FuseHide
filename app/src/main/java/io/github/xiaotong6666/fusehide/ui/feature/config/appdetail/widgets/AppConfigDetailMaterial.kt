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

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.uihelper.common.StatusTag
import io.github.xiaotong6666.uihelper.material.materialSurfaceLadder
import io.github.xiaotong6666.uihelper.material.primitive.ExpressiveSwitchMaterial
import io.github.xiaotong6666.uihelper.material.primitive.SegmentedItemContainer
import io.github.xiaotong6666.uihelper.material.primitive.SegmentedListItem

@Composable
fun AppConfigInfoCardMaterial(
    label: String,
    packageName: String,
    versionName: String,
    versionCode: Long,
    uid: Int,
    modifier: Modifier = Modifier,
    appIcon: (@Composable () -> Unit)? = null,
) {
    val surfaces = materialSurfaceLadder()
    val userId = uid / 100000
    val appId = if (uid >= 0) uid % 100000 else -1
    SegmentedItemContainer(
        modifier = modifier,
        containerColor = surfaces.grouped,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            appIcon?.invoke()
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(text = label, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (versionName.isNotBlank()) {
                    Text(
                        text = "$versionName ($versionCode)",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
                Text(
                    text = packageName,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            if (uid >= 0) {
                Column(horizontalAlignment = Alignment.End, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    if (userId != 0) {
                        StatusTag(
                            label = "USER $userId",
                            backgroundColor = MaterialTheme.colorScheme.tertiaryContainer,
                            contentColor = MaterialTheme.colorScheme.onTertiaryContainer,
                        )
                        StatusTag(
                            label = "UID $appId",
                            backgroundColor = MaterialTheme.colorScheme.tertiaryContainer,
                            contentColor = MaterialTheme.colorScheme.onTertiaryContainer,
                        )
                    } else {
                        StatusTag(
                            label = "UID $uid",
                            backgroundColor = MaterialTheme.colorScheme.tertiaryContainer,
                            contentColor = MaterialTheme.colorScheme.onTertiaryContainer,
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun AppConfigToggleCardMaterial(
    checked: Boolean,
    title: String,
    description: String,
    modifier: Modifier = Modifier,
    onToggle: () -> Unit,
) {
    val surfaces = materialSurfaceLadder()
    SegmentedListItem(
        selected = checked,
        onClick = onToggle,
        modifier = modifier,
        colors = ListItemDefaults.segmentedColors(
            containerColor = if (checked) surfaces.groupedSelected else surfaces.grouped,
            disabledContainerColor = if (checked) surfaces.groupedSelected else surfaces.grouped,
            supportingContentColor = if (checked) surfaces.groupedSelectedContent.copy(alpha = 0.86f) else MaterialTheme.colorScheme.onSurfaceVariant,
        ),
        headlineContent = {
            Text(text = title, style = MaterialTheme.typography.titleMedium)
        },
        supportingContent = {
            Text(
                text = description,
                style = MaterialTheme.typography.bodyMedium,
                color = if (checked) surfaces.groupedSelectedContent.copy(alpha = 0.86f) else MaterialTheme.colorScheme.onSurfaceVariant,
            )
        },
        trailingContent = {
            ExpressiveSwitchMaterial(
                checked = checked,
                onCheckedChange = null,
                modifier = Modifier.clearAndSetSemantics {},
            )
        },
    )
}

@Composable
fun AppConfigTargetsCardMaterial(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    description: String,
    modifier: Modifier = Modifier,
    minLines: Int = 5,
    maxLines: Int = 8,
) {
    val surfaces = materialSurfaceLadder()
    SegmentedItemContainer(
        modifier = modifier,
        containerColor = surfaces.grouped,
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            Text(text = label, style = MaterialTheme.typography.titleMedium)
            BasicTextField(
                value = value,
                onValueChange = onValueChange,
                modifier = Modifier.fillMaxWidth(),
                minLines = minLines,
                maxLines = maxLines,
                textStyle = MaterialTheme.typography.bodyLarge.copy(color = MaterialTheme.colorScheme.onSurface),
                cursorBrush = SolidColor(MaterialTheme.colorScheme.primary),
                decorationBox = { innerTextField ->
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .heightIn(min = (minLines * 24).dp)
                            .clip(MaterialTheme.shapes.large)
                            .background(surfaces.input)
                            .padding(vertical = 4.dp)
                            .padding(horizontal = 14.dp, vertical = 12.dp),
                    ) {
                        innerTextField()
                    }
                },
            )
            Text(text = description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
