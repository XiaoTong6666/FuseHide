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

package io.github.xiaotong6666.uihelper.extensions.androidapp

import android.content.pm.ApplicationInfo
import android.content.pm.PackageInfo
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import top.yukonga.miuix.kmp.theme.MiuixTheme

private data class IconKey(val uid: Int, val packageName: String, val sourceDir: String?)

@Composable
fun AppIconImage(
    modifier: Modifier = Modifier,
    applicationInfo: ApplicationInfo,
    label: String,
) {
    val density = LocalDensity.current
    val context = LocalContext.current
    val targetSizePx = with(density) { 48.dp.roundToPx() }

    val iconKey = IconKey(applicationInfo.uid, applicationInfo.packageName, applicationInfo.sourceDir)

    Box(modifier = modifier) {
        val initiallyCached = remember(iconKey) {
            AppIconCache.getCached(applicationInfo, targetSizePx) != null
        }

        var appBitmap by remember(iconKey) {
            mutableStateOf(AppIconCache.getCached(applicationInfo, targetSizePx))
        }

        if (!initiallyCached) {
            LaunchedEffect(iconKey) {
                appBitmap = AppIconCache.loadIcon(context, applicationInfo, targetSizePx)
            }
        }

        val icon = appBitmap
        if (initiallyCached && icon != null) {
            val imageBitmap = remember(icon) { icon.asImageBitmap() }
            Image(
                bitmap = imageBitmap,
                contentDescription = label,
                modifier = Modifier.fillMaxSize(),
            )
        } else {
            Crossfade(
                targetState = icon,
                animationSpec = tween(durationMillis = 150),
                label = "IconFade",
            ) { bitmap ->
                if (bitmap != null) {
                    val imageBitmap = remember(bitmap) { bitmap.asImageBitmap() }
                    Image(
                        bitmap = imageBitmap,
                        contentDescription = label,
                        modifier = Modifier.fillMaxSize(),
                    )
                } else {
                    PlaceHolderBox(Modifier.fillMaxSize())
                }
            }
        }
    }
}

@Composable
fun AppIconImage(
    modifier: Modifier = Modifier,
    packageInfo: PackageInfo,
    label: String,
) {
    val appInfo = packageInfo.applicationInfo
    if (appInfo == null) {
        PlaceHolderBox(modifier)
        return
    }
    AppIconImage(modifier, appInfo, label)
}

@Composable
private fun PlaceHolderBox(modifier: Modifier = Modifier) {
    val containerColor = when (LocalUiMode.current) {
        UiMode.Material -> MaterialTheme.colorScheme.secondaryContainer
        UiMode.Miuix -> MiuixTheme.colorScheme.secondaryContainer
    }

    Box(
        modifier = modifier
            .padding(4.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(containerColor),
    )
}
