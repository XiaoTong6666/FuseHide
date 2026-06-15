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

import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalLayoutDirection
import top.yukonga.miuix.kmp.basic.MiuixScrollBehavior
import top.yukonga.miuix.kmp.theme.MiuixTheme
import top.yukonga.miuix.kmp.utils.overScrollVertical

@Composable
internal fun MiuixPageScaffold(
    title: String,
    subtitle: String,
    outerPadding: PaddingValues,
    isCurrentPage: Boolean,
    content: @Composable (contentPadding: PaddingValues, scrollModifier: Modifier) -> Unit,
) {
    val layoutDirection = LocalLayoutDirection.current
    val scrollBehavior = MiuixScrollBehavior()

    top.yukonga.miuix.kmp.basic.Scaffold(
        topBar = {
            top.yukonga.miuix.kmp.basic.TopAppBar(
                title = title,
                color = MiuixTheme.colorScheme.surface,
                titleColor = MiuixTheme.colorScheme.onSurface,
                subtitle = subtitle,
                subtitleColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                scrollBehavior = scrollBehavior,
            )
        },
    ) { innerPadding ->
        content(
            PaddingValues(
                start = outerPadding.calculateStartPadding(layoutDirection),
                top = innerPadding.calculateTopPadding(),
                end = outerPadding.calculateEndPadding(layoutDirection),
                bottom = outerPadding.calculateBottomPadding(),
            ),
            Modifier
                .then(if (isCurrentPage) Modifier.overScrollVertical() else Modifier)
                .nestedScroll(scrollBehavior.nestedScrollConnection),
        )
    }
}
