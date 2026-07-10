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

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.uihelper.miuix.primitive.SearchBarFake

@Composable
fun AppListSearchFieldMiuix(
    label: String,
    modifier: Modifier = Modifier,
    dynamicTopPadding: Dp = 12.dp,
) {
    SearchBarFake(
        label = label,
        modifier = modifier,
        searchBarTopPadding = dynamicTopPadding,
        bottomPadding = 0.dp,
    )
}
