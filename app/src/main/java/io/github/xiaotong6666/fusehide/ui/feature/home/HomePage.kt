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

package io.github.xiaotong6666.fusehide.ui.feature.home

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Android
import androidx.compose.material.icons.filled.DeveloperBoard
import androidx.compose.material.icons.filled.FilterList
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Smartphone
import androidx.compose.material.icons.filled.Sync
import androidx.compose.material.icons.rounded.CheckCircleOutline
import androidx.compose.material.icons.rounded.ErrorOutline
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.contentColorFor
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.core.model.HomeCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.HookBackend
import io.github.xiaotong6666.fusehide.ui.core.model.HookStatusUiState
import io.github.xiaotong6666.uihelper.adaptive.HomeInfoCard
import io.github.xiaotong6666.uihelper.adaptive.HomeStatusCard
import io.github.xiaotong6666.uihelper.common.StatusTag
import io.github.xiaotong6666.uihelper.material.primitive.SegmentedColumn
import io.github.xiaotong6666.uihelper.material.primitive.SegmentedListItem
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import io.github.xiaotong6666.uihelper.model.HomeInfoItem
import top.yukonga.miuix.kmp.basic.BasicComponent
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.utils.overScrollVertical
import top.yukonga.miuix.kmp.utils.scrollEndHaptic
import androidx.compose.material3.Icon as MaterialIcon
import androidx.compose.material3.Text as MaterialText

@Composable
fun HomePage(
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    callbacks: HomeCallbacks,
    contentPadding: PaddingValues,
    isCurrentPage: Boolean = true,
    modifier: Modifier = Modifier,
) {
    if (LocalUiMode.current == UiMode.Miuix) {
        HomePageContentMiuix(
            hookStatus = hookStatus,
            configState = configState,
            callbacks = callbacks,
            contentPadding = contentPadding,
            modifier = modifier,
        )
        return
    }

    HomePageContent(
        hookStatus = hookStatus,
        configState = configState,
        callbacks = callbacks,
        contentPadding = contentPadding,
        modifier = modifier,
    )
}

@Composable
private fun HomePageContentMiuix(
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    callbacks: HomeCallbacks,
    contentPadding: PaddingValues,
    modifier: Modifier,
) {
    val layoutDirection = LocalLayoutDirection.current
    val rawInfoPairs = remember(hookStatus.infoText) { parseHomeInfo(hookStatus.infoText) }
    val version = rawInfoPairs.firstOrNull { it.first == "Version" }?.second.orEmpty()
    val checking = !hookStatus.hookCheckCompleted
    val hookTitle = when {
        checking -> stringResource(R.string.state_checking_short)
        hookStatus.isHooked -> stringResource(R.string.home_working)
        else -> stringResource(R.string.home_not_working)
    }
    val hookSummary = if (version.isNotBlank()) {
        stringResource(R.string.home_working_version, version)
    } else {
        hookSummarySupportingText(
            isHooked = hookStatus.isHooked,
            hookCheckCompleted = hookStatus.hookCheckCompleted,
            hookedPackage = hookStatus.hookedPackage,
        )
    }
    val hookFooter = when (hookStatus.backend) {
        HookBackend.Xposed -> stringResource(R.string.home_backend_xposed)
        HookBackend.Zygisk -> stringResource(R.string.home_backend_zygisk)
        null -> ""
    }

    LazyColumn(
        modifier = Modifier
            .fillMaxHeight()
            .scrollEndHaptic()
            .overScrollVertical()
            .then(modifier)
            .padding(horizontal = 12.dp),
        contentPadding = PaddingValues(
            top = contentPadding.calculateTopPadding() + 12.dp,
            bottom = contentPadding.calculateBottomPadding() + 12.dp,
            start = contentPadding.calculateStartPadding(layoutDirection),
            end = contentPadding.calculateEndPadding(layoutDirection),
        ),
        overscrollEffect = null,
    ) {
        item {
            androidx.compose.foundation.layout.Column(
                verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp),
            ) {
                HomeStatusCard(
                    title = hookTitle,
                    summary = hookSummary,
                    footer = hookFooter,
                    healthy = hookStatus.isHooked,
                    checking = checking,
                    onClick = callbacks.onStatusClick,
                )
                RuntimeDetailsCardMiuix(
                    hookStatus = hookStatus,
                    configState = configState,
                    checking = checking,
                    onConfigSyncClick = callbacks.onConfigSyncClick,
                )
                HomeInfoCard(items = homeMainInfo(rawInfoPairs))
                HomeInfoCard(items = homeCapabilityInfo(rawInfoPairs))
            }
        }
    }
}

@Composable
private fun RuntimeDetailsCardMiuix(
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    checking: Boolean,
    onConfigSyncClick: () -> Unit,
) {
    val processPackage = hookStatus.hookedPackage
        ?.takeIf { it.isNotBlank() }
        ?: if (checking) {
            stringResource(R.string.home_process_checking)
        } else {
            stringResource(R.string.home_process_not_found)
        }
    val processPid = if (hookStatus.hookedPid > 0) {
        stringResource(R.string.status_pid, hookStatus.hookedPid)
    } else {
        ""
    }
    val syncChecking = configState.appliedConfigQueryPending
    val syncUnavailable = !syncChecking && !configState.hasAppliedConfig
    val syncNeedsReview = !syncChecking &&
        configState.hasAppliedConfig &&
        configState.draftVsAppliedDiff.hasDifferences
    val syncLabel = when {
        syncChecking -> stringResource(R.string.state_checking_short)
        syncUnavailable -> stringResource(R.string.state_sync_unavailable)
        syncNeedsReview -> stringResource(R.string.state_sync_needs_review)
        else -> stringResource(R.string.state_sync_ok)
    }
    val syncSummary = when {
        syncChecking -> stringResource(R.string.config_snapshot_waiting)

        syncUnavailable -> stringResource(R.string.config_snapshot_missing)

        else -> configState.draftVsAppliedDiff.summary.ifBlank {
            configState.configStatusText.trim()
        }
    }

    Card {
        BasicComponent(
            startAction = {
                Icon(
                    imageVector = Icons.Filled.Memory,
                    contentDescription = null,
                    modifier = Modifier.padding(end = 6.dp),
                    tint = MiuixTheme.colorScheme.onBackground,
                )
            },
            endActions = {
                if (checking) {
                    StatusTag(
                        label = stringResource(R.string.state_checking_short),
                        backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
                        contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest,
                    )
                }
            },
        ) {
            Text(
                text = stringResource(R.string.home_media_provider),
                fontSize = MiuixTheme.textStyles.headline1.fontSize,
                fontWeight = FontWeight.Medium,
                color = MiuixTheme.colorScheme.onBackground,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = processPackage,
                fontSize = MiuixTheme.textStyles.body2.fontSize,
                color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                minLines = 1,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                text = processPid,
                fontSize = MiuixTheme.textStyles.body2.fontSize,
                color = if (processPid.isEmpty()) {
                    androidx.compose.ui.graphics.Color.Transparent
                } else {
                    MiuixTheme.colorScheme.onSurfaceVariantSummary
                },
                minLines = 1,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .clickable(onClick = onConfigSyncClick),
        ) {
            BasicComponent(
                startAction = {
                    Icon(
                        imageVector = Icons.Filled.Sync,
                        contentDescription = null,
                        modifier = Modifier.padding(end = 6.dp),
                        tint = MiuixTheme.colorScheme.onBackground,
                    )
                },
                endActions = {
                    StatusTag(
                        label = syncLabel,
                        backgroundColor = when {
                            syncChecking -> MiuixTheme.colorScheme.surfaceContainerHighest
                            syncUnavailable || syncNeedsReview -> MiuixTheme.colorScheme.errorContainer
                            else -> MiuixTheme.colorScheme.secondaryContainer.copy(alpha = 0.8f)
                        },
                        contentColor = when {
                            syncChecking -> MiuixTheme.colorScheme.onSurfaceContainerHighest
                            syncUnavailable || syncNeedsReview -> MiuixTheme.colorScheme.onErrorContainer
                            else -> MiuixTheme.colorScheme.onSecondaryContainer
                        },
                    )
                },
            ) {
                Text(
                    text = stringResource(R.string.home_config_sync),
                    fontSize = MiuixTheme.textStyles.headline1.fontSize,
                    fontWeight = FontWeight.Medium,
                    color = MiuixTheme.colorScheme.onBackground,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    text = syncSummary,
                    fontSize = MiuixTheme.textStyles.body2.fontSize,
                    color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                    minLines = 2,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}

@Composable
private fun HomePageContent(
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    callbacks: HomeCallbacks,
    contentPadding: PaddingValues,
    modifier: Modifier,
) {
    val rawInfoPairs = remember(hookStatus.infoText) { parseHomeInfo(hookStatus.infoText) }
    val version = rawInfoPairs.firstOrNull { it.first == "Version" }?.second.orEmpty()

    Column(
        modifier = modifier
            .padding(contentPadding)
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp),
        verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(13.dp),
    ) {
        HomeStatusCardMaterialKsu(
            hookStatus = hookStatus,
            version = version,
            onClick = callbacks.onStatusClick,
        )

        RuntimeDetailsMaterialKsu(
            hookStatus = hookStatus,
            configState = configState,
            onConfigSyncClick = callbacks.onConfigSyncClick,
        )

        HomeInfoSegmentedMaterial(items = homeMainInfo(rawInfoPairs))
        HomeInfoSegmentedMaterial(items = homeCapabilityInfo(rawInfoPairs))
    }
}

@Composable
private fun HomeStatusCardMaterialKsu(
    hookStatus: HookStatusUiState,
    version: String,
    onClick: () -> Unit,
) {
    val checking = !hookStatus.hookCheckCompleted
    val containerColor = when {
        checking -> MaterialTheme.colorScheme.surfaceContainerHigh
        hookStatus.isHooked -> MaterialTheme.colorScheme.secondaryContainer
        else -> MaterialTheme.colorScheme.errorContainer
    }
    val contentColor = contentColorFor(containerColor)
    val title = when {
        checking -> stringResource(R.string.state_checking_short)
        hookStatus.isHooked -> stringResource(R.string.home_working)
        else -> stringResource(R.string.home_not_working)
    }
    val summary = if (version.isNotBlank()) {
        stringResource(R.string.home_working_version, version)
    } else {
        hookSummarySupportingText(
            isHooked = hookStatus.isHooked,
            hookCheckCompleted = hookStatus.hookCheckCompleted,
            hookedPackage = hookStatus.hookedPackage,
        )
    }
    val backend = when (hookStatus.backend) {
        HookBackend.Xposed -> stringResource(R.string.home_backend_xposed)
        HookBackend.Zygisk -> stringResource(R.string.home_backend_zygisk)
        null -> null
    }

    Surface(
        modifier = Modifier.fillMaxWidth(),
        color = containerColor,
        contentColor = contentColor,
        shape = MaterialTheme.shapes.large,
        onClick = onClick,
    ) {
        ListItem(
            leadingContent = {
                MaterialIcon(
                    imageVector = when {
                        checking -> Icons.Filled.Sync
                        hookStatus.isHooked -> Icons.Rounded.CheckCircleOutline
                        else -> Icons.Rounded.ErrorOutline
                    },
                    contentDescription = title,
                )
            },
            trailingContent = backend?.let { label ->
                {
                    StatusTag(
                        label = label,
                        backgroundColor = MaterialTheme.colorScheme.primary,
                        contentColor = MaterialTheme.colorScheme.onPrimary,
                    )
                }
            },
            supportingContent = {
                MaterialText(
                    text = summary,
                    style = MaterialTheme.typography.bodyMedium,
                    color = contentColor.copy(alpha = 0.7f),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            },
            colors = ListItemDefaults.colors(
                containerColor = Color.Transparent,
                headlineColor = contentColor,
                leadingIconColor = contentColor,
                trailingIconColor = contentColor,
                supportingColor = contentColor.copy(alpha = 0.7f),
            ),
        ) {
            MaterialText(
                text = title,
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Medium,
            )
        }
    }
}

@Composable
private fun RuntimeDetailsMaterialKsu(
    hookStatus: HookStatusUiState,
    configState: ConfigUiState,
    onConfigSyncClick: () -> Unit,
) {
    val checking = !hookStatus.hookCheckCompleted
    val processPackage = hookStatus.hookedPackage
        ?.takeIf { it.isNotBlank() }
        ?: if (checking) {
            stringResource(R.string.home_process_checking)
        } else {
            stringResource(R.string.home_process_not_found)
        }
    val processPid = if (hookStatus.hookedPid > 0) {
        stringResource(R.string.status_pid, hookStatus.hookedPid)
    } else {
        ""
    }

    val syncChecking = configState.appliedConfigQueryPending
    val syncUnavailable = !syncChecking && !configState.hasAppliedConfig
    val syncNeedsReview = !syncChecking &&
        configState.hasAppliedConfig &&
        configState.draftVsAppliedDiff.hasDifferences
    val syncLabel = when {
        syncChecking -> stringResource(R.string.state_checking_short)
        syncUnavailable -> stringResource(R.string.state_sync_unavailable)
        syncNeedsReview -> stringResource(R.string.state_sync_needs_review)
        else -> stringResource(R.string.state_sync_ok)
    }
    val syncSummary = when {
        syncChecking -> stringResource(R.string.config_snapshot_waiting)

        syncUnavailable -> stringResource(R.string.config_snapshot_missing)

        else -> configState.draftVsAppliedDiff.summary.ifBlank {
            configState.configStatusText.trim()
        }
    }

    SegmentedColumn(modifier = Modifier.fillMaxWidth()) {
        item {
            SegmentedListItem(
                headlineContent = {
                    MaterialText(
                        text = stringResource(R.string.home_media_provider),
                        style = MaterialTheme.typography.bodyLarge,
                    )
                },
                supportingContent = {
                    Column {
                        MaterialText(
                            text = processPackage,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            minLines = 1,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                        MaterialText(
                            text = processPid,
                            style = MaterialTheme.typography.bodyMedium,
                            color = if (processPid.isEmpty()) {
                                Color.Transparent
                            } else {
                                MaterialTheme.colorScheme.onSurfaceVariant
                            },
                            minLines = 1,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                },
                leadingContent = {
                    MaterialIcon(
                        imageVector = Icons.Filled.Memory,
                        contentDescription = stringResource(R.string.home_media_provider),
                    )
                },
                trailingContent = if (checking) {
                    {
                        StatusTag(
                            label = stringResource(R.string.state_checking_short),
                            backgroundColor = MaterialTheme.colorScheme.surfaceContainerHighest,
                            contentColor = MaterialTheme.colorScheme.onSurface,
                        )
                    }
                } else {
                    null
                },
            )
        }
        item {
            SegmentedListItem(
                onClick = onConfigSyncClick,
                headlineContent = {
                    MaterialText(
                        text = stringResource(R.string.home_config_sync),
                        style = MaterialTheme.typography.bodyLarge,
                    )
                },
                supportingContent = {
                    MaterialText(
                        text = syncSummary,
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        minLines = 2,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                    )
                },
                leadingContent = {
                    MaterialIcon(
                        imageVector = Icons.Filled.Sync,
                        contentDescription = stringResource(R.string.home_config_sync),
                    )
                },
                trailingContent = {
                    StatusTag(
                        label = syncLabel,
                        backgroundColor = when {
                            syncChecking -> MaterialTheme.colorScheme.surfaceContainerHighest
                            syncUnavailable || syncNeedsReview -> MaterialTheme.colorScheme.errorContainer
                            else -> MaterialTheme.colorScheme.primary
                        },
                        contentColor = when {
                            syncChecking -> MaterialTheme.colorScheme.onSurface
                            syncUnavailable || syncNeedsReview -> MaterialTheme.colorScheme.onErrorContainer
                            else -> MaterialTheme.colorScheme.onPrimary
                        },
                    )
                },
            )
        }
    }
}

@Composable
private fun HomeInfoSegmentedMaterial(
    items: List<HomeInfoItem>,
) {
    if (items.isEmpty()) return

    SegmentedColumn(modifier = Modifier.fillMaxWidth()) {
        items.forEachIndexed { index, item ->
            item(key = "${item.title}:$index") {
                SegmentedListItem(
                    headlineContent = {
                        MaterialText(
                            text = item.title,
                            style = MaterialTheme.typography.bodyLarge,
                        )
                    },
                    supportingContent = {
                        MaterialText(
                            text = item.value,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    },
                    leadingContent = {
                        MaterialIcon(
                            imageVector = item.icon,
                            contentDescription = item.title,
                        )
                    },
                )
            }
        }
    }
}

private fun parseHomeInfo(infoText: String): List<Pair<String, String>> = infoText.lines()
    .filter { it.isNotBlank() }
    .map { line ->
        val parts = line.split(":", limit = 2)
        if (parts.size == 2) {
            parts[0].trim() to parts[1].trim()
        } else {
            "Info" to line.trim()
        }
    }

@Composable
private fun homeMainInfo(rawInfoPairs: List<Pair<String, String>>): List<HomeInfoItem> = rawInfoPairs.mapNotNull { (key, value) ->
    when (key) {
        "Kernel" -> HomeInfoItem(
            icon = Icons.Filled.DeveloperBoard,
            title = stringResource(R.string.home_kernel_version),
            value = value,
        )

        "Device" -> HomeInfoItem(
            icon = Icons.Filled.Smartphone,
            title = stringResource(R.string.home_device_model),
            value = value,
        )

        "System" -> HomeInfoItem(
            icon = Icons.Filled.Android,
            title = stringResource(R.string.home_system),
            value = value,
        )

        else -> null
    }
}

private fun homeCapabilityInfo(rawInfoPairs: List<Pair<String, String>>): List<HomeInfoItem> = rawInfoPairs.mapNotNull { (key, value) ->
    when (key) {
        "Version", "Kernel", "Device", "System" -> null

        else -> HomeInfoItem(
            icon = if (key.contains("fuse", ignoreCase = true)) {
                Icons.Filled.FilterList
            } else {
                Icons.Filled.Security
            },
            title = key,
            value = value,
        )
    }
}
