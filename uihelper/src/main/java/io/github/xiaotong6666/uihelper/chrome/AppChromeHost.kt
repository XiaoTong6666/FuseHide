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

package io.github.xiaotong6666.uihelper.chrome

import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.RowScope
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.Stable
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.NestedScrollConnection
import androidx.compose.ui.input.nestedscroll.nestedScroll

typealias ComposableContent = @Composable () -> Unit
typealias MiuixTopBarWrapper = @Composable (content: ComposableContent) -> Unit

data class AppChromeSpec(
    val materialTopBar: (@Composable () -> Unit)? = null,
    val miuixTopBar: (@Composable () -> Unit)? = null,
    val miuixTopBarWrapper: MiuixTopBarWrapper? = null,
    val materialActions: (@Composable RowScope.() -> Unit)? = null,
    val miuixActions: (@Composable () -> Unit)? = null,
    val miuixPopupHost: (@Composable () -> Unit)? = null,
    val overlayContent: (@Composable (PaddingValues) -> Unit)? = null,
    val hideBottomBar: Boolean = false,
    val consumeOuterScroll: Boolean = false,
)

internal class AppChromeState {
    private var owner: Any? = null
    var spec by mutableStateOf(AppChromeSpec())
        private set

    fun set(owner: Any, spec: AppChromeSpec) {
        this.owner = owner
        this.spec = spec
    }

    fun clear(owner: Any) {
        if (this.owner === owner) {
            this.owner = null
            this.spec = AppChromeSpec()
        }
    }
}

@Stable
class PageHostHandle internal constructor(
    private val nestedScrollConnection: NestedScrollConnection? = null,
    private val collapsedFractionProvider: () -> Float = { 0f },
) {
    val collapsedFraction: Float
        get() = collapsedFractionProvider().coerceIn(0f, 1f)

    fun nestedScroll(modifier: Modifier = Modifier): Modifier = if (nestedScrollConnection != null) {
        modifier.nestedScroll(nestedScrollConnection)
    } else {
        modifier
    }
}

private val EmptyPageHostHandle = PageHostHandle()

internal val LocalAppChromeState = compositionLocalOf<AppChromeState?> { null }
internal val LocalMaterialNestedScrollConnection = compositionLocalOf<NestedScrollConnection?> { null }
internal val LocalMiuixNestedScrollConnection = compositionLocalOf<NestedScrollConnection?> { null }
internal val LocalMiuixCollapsedFractionProvider = compositionLocalOf<(() -> Float)?> { null }
internal val LocalPageHostHandle = compositionLocalOf { EmptyPageHostHandle }

@Composable
internal fun rememberAppChromeState(): AppChromeState = remember { AppChromeState() }

@Composable
fun rememberPageHostHandle(): PageHostHandle = LocalPageHostHandle.current

@Composable
fun PageChrome(spec: AppChromeSpec, enabled: Boolean = true) {
    val appChromeState = LocalAppChromeState.current ?: return
    val owner = remember { Any() }

    if (!enabled) {
        DisposableEffect(appChromeState, owner) {
            appChromeState.clear(owner)
            onDispose {
                appChromeState.clear(owner)
            }
        }
        return
    }

    SideEffect {
        appChromeState.set(owner, spec)
    }

    DisposableEffect(appChromeState, owner) {
        onDispose {
            appChromeState.clear(owner)
        }
    }
}

@Composable
fun PageHost(
    enabled: Boolean = true,
    spec: (PageHostHandle) -> AppChromeSpec = { AppChromeSpec() },
    content: @Composable (PageHostHandle) -> Unit,
) {
    val pageHostHandle = rememberPageHostHandle()
    PageChrome(spec(pageHostHandle), enabled)
    content(pageHostHandle)
}
