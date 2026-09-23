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

import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.uihelper.adaptive.InfoPanel
import io.github.xiaotong6666.uihelper.chrome.DetailPageBody
import io.github.xiaotong6666.uihelper.chrome.DetailPageHost

@Composable
fun AppliedConfigPage(
    snapshotText: String,
    onBack: () -> Unit,
) {
    DetailPageHost(
        title = stringResource(R.string.home_applied_config_title),
        onBack = onBack,
    ) { contentPadding, scrollModifier ->
        DetailPageBody(
            contentPadding = contentPadding,
            scrollModifier = scrollModifier,
        ) {
            InfoPanel(
                title = stringResource(R.string.home_backend_config),
                text = snapshotText.ifBlank {
                    stringResource(R.string.config_snapshot_missing)
                },
                monospace = true,
            )
        }
    }
}
