package io.github.xiaotong6666.fusehide.ui.navigation3

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable
import top.yukonga.miuix.kmp.nav.core.NavKey

@Serializable
sealed interface Route :
    NavKey,
    Parcelable {
    @Parcelize
    @Serializable
    data object Main : Route

    @Parcelize
    @Serializable
    data object GlobalConfig : Route

    @Parcelize
    @Serializable
    data class AppConfig(val packageName: String) : Route
}
