package io.github.xiaotong6666.uihelper.navigation3

import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.saveable.Saver
import androidx.compose.runtime.saveable.listSaver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.navigation3.runtime.NavKey

class Navigator(initialKey: NavKey) {
    val backStack: SnapshotStateList<NavKey> = mutableStateListOf(initialKey)

    fun push(key: NavKey) {
        backStack.add(key)
    }

    fun pop() {
        if (backStack.isNotEmpty()) {
            backStack.removeAt(backStack.lastIndex)
        }
    }

    fun current(): NavKey? = backStack.lastOrNull()

    companion object {
        fun saver(fallbackRoute: NavKey): Saver<Navigator, Any> = listSaver(
            save = { navigator -> navigator.backStack.toList() },
            restore = { savedList ->
                val initialKey = savedList.firstOrNull() ?: fallbackRoute
                val navigator = Navigator(initialKey)
                navigator.backStack.clear()
                if (savedList.isEmpty()) {
                    navigator.backStack.add(fallbackRoute)
                } else {
                    navigator.backStack.addAll(savedList)
                }
                navigator
            },
        )
    }
}

@Composable
fun rememberNavigator(
    startRoute: NavKey,
    fallbackRoute: NavKey = startRoute,
): Navigator = rememberSaveable(startRoute, fallbackRoute, saver = Navigator.saver(fallbackRoute)) {
    Navigator(startRoute)
}

val LocalNavigator = staticCompositionLocalOf<Navigator> {
    error("LocalNavigator not provided")
}
