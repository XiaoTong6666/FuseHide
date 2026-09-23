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

package io.github.xiaotong6666.fusehide.ui.app

import android.annotation.SuppressLint
import android.content.BroadcastReceiver
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.content.IntentFilter
import android.os.Binder
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.system.Os
import android.system.StructUtsname
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.core.content.ContextCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.xiaotong6666.fusehide.BuildConfig
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.config.HideConfigDefaults
import io.github.xiaotong6666.fusehide.config.HideConfigStore
import io.github.xiaotong6666.fusehide.config.buildAppliedConfigSnapshot
import io.github.xiaotong6666.fusehide.config.buildDraftVsAppliedDiff
import io.github.xiaotong6666.fusehide.config.formatNow
import io.github.xiaotong6666.fusehide.config.hasDraftVsAppliedDifferences
import io.github.xiaotong6666.fusehide.debug.PathDebugActions
import io.github.xiaotong6666.fusehide.debug.PathDebugText
import io.github.xiaotong6666.fusehide.status.HookStatusProbe
import io.github.xiaotong6666.fusehide.status.StatusBroadcastReceiver
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.core.model.DebugCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.DebugUiState
import io.github.xiaotong6666.fusehide.ui.core.model.HomeCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.HookBackend
import io.github.xiaotong6666.fusehide.ui.core.model.HookStatusUiState
import io.github.xiaotong6666.fusehide.ui.core.model.SettingsCallbacks
import io.github.xiaotong6666.fusehide.ui.core.model.SettingsUiState
import io.github.xiaotong6666.fusehide.ui.feature.config.appdetail.AppConfigPage
import io.github.xiaotong6666.fusehide.ui.feature.config.applist.AppListViewModel
import io.github.xiaotong6666.fusehide.ui.feature.config.global.GlobalConfigPage
import io.github.xiaotong6666.fusehide.ui.navigation3.Route
import io.github.xiaotong6666.fusehide.ui.theme.FuseHideTheme
import io.github.xiaotong6666.uihelper.miuix.effect.LocalMiuixBlurEnabled
import io.github.xiaotong6666.uihelper.mode.LocalUiMode
import io.github.xiaotong6666.uihelper.mode.UiMode
import io.github.xiaotong6666.uihelper.navigation3.LocalNavigator
import io.github.xiaotong6666.uihelper.navigation3.rememberNavigator
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import top.yukonga.miuix.kmp.nav.core.NavDisplay
import top.yukonga.miuix.kmp.nav.core.NavDisplayEffects
import top.yukonga.miuix.kmp.nav.core.rememberNavSystemCornerRadius
import top.yukonga.miuix.kmp.nav.transition.NavSwipeDirection
import java.lang.ref.WeakReference
import java.util.UUID

class MainActivity :
    ComponentActivity(),
    StatusBroadcastReceiver.HookStatusCallback {
    companion object {
        private const val EXTRA_DEBUG_PATH = "debug_path"
        private const val EXTRA_DEBUG_PATH2 = "debug_path2"
        private const val EXTRA_DEBUG_ACTIONS = "debug_actions"
        private const val APPLIED_CONFIG_QUERY_TIMEOUT_MS = 2000L

        @SuppressLint("PrivateApi")
        fun getBooleanSystemProperty(name: String): Boolean = try {
            Class.forName("android.os.SystemProperties")
                .getDeclaredMethod("getBoolean", String::class.java, Boolean::class.javaPrimitiveType)
                .invoke(null, name, false) as Boolean
        } catch (th: Throwable) {
            Log.e("FuseHide", "getProp", th)
            false
        }

        @SuppressLint("PrivateApi")
        fun getSystemProperty(name: String): String = try {
            Class.forName("android.os.SystemProperties")
                .getDeclaredMethod("get", String::class.java, String::class.java)
                .invoke(null, name, "") as String
        } catch (th: Throwable) {
            Log.e("FuseHide", "getProp", th)
            ""
        }
    }

    private val mainViewModel by viewModels<MainActivityViewModel>()

    private var statusBinderReference: WeakReference<Binder>? = null
    private var statusCheckInFlight: Boolean = false
    private var statusTimeoutRunnable: Runnable? = null
    private var appliedConfigTimeoutRunnable: Runnable? = null
    private var activeStatusCheckToken: String? = null
    private val mainHandler = Handler(Looper.getMainLooper())
    private lateinit var statusReceiver: StatusBroadcastReceiver
    private lateinit var configStatusReceiver: BroadcastReceiver
    private lateinit var appliedConfigReceiver: BroadcastReceiver
    private val hookStatusProbe by lazy {
        HookStatusProbe(
            context = this,
            onTimeout = { token ->
                if (token != activeStatusCheckToken) {
                    Log.d("FuseHide", "ignore stale timeout token=$token active=$activeStatusCheckToken")
                    return@HookStatusProbe
                }
                statusBinderReference = null
                statusTimeoutRunnable = null
                activeStatusCheckToken = null
                onHookCheckTimeout()
            },
            onStarted = { binderReference, timeoutRunnable ->
                statusBinderReference = binderReference
                statusTimeoutRunnable = timeoutRunnable
            },
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val uiMode = FuseHideUiModeStore.fromPrefs(this)
        val enableMiuixBlur = FuseHideUiModeStore.isMiuixBlurEnabled(this)
        val enableMiuixFloatingBottomBar = FuseHideUiModeStore.isMiuixFloatingBottomBarEnabled(this)
        val infoText = buildInfoText()
        val savedConfig = HideConfigStore.loadSavedConfigOrNull(this)
        mainViewModel.initialize(
            uiMode = uiMode,
            enableMiuixBlur = enableMiuixBlur,
            enableMiuixFloatingBottomBar = enableMiuixFloatingBottomBar,
            infoText = infoText,
            savedConfig = savedConfig,
            configStatusText = getString(
                if (savedConfig == null) R.string.config_missing_local else R.string.config_loaded_saved,
            ) + "\n",
            appliedConfigSnapshotText = getString(R.string.config_snapshot_missing) + "\n",
        )

        statusReceiver = StatusBroadcastReceiver(this, 1, this)
        val filter = IntentFilter(HookStatusProbe.ACTION_SET_STATUS)
        ContextCompat.registerReceiver(this, statusReceiver, filter, HookStatusProbe.registerReceiverFlags())

        configStatusReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: android.content.Context?, intent: Intent?) {
                val statusIntent = intent ?: return
                val token = statusIntent.getStringExtra(HideConfigStore.EXTRA_RELOAD_TOKEN) ?: return
                if (!mainViewModel.consumeReloadAck(token)) {
                    return
                }
                val applied = statusIntent.getBooleanExtra(HideConfigStore.EXTRA_RELOAD_APPLIED, false)
                val message = statusIntent.getStringExtra(HideConfigStore.EXTRA_RELOAD_MESSAGE) ?: "unknown"
                val statusText = if (applied) {
                    refreshAppliedConfig(autoScrollToResults = true)
                    getString(R.string.config_applied_ok) + "\n"
                } else {
                    getString(R.string.config_applied_fail, message) + "\n"
                }
                mainViewModel.applyReloadAck(
                    token = token,
                    resultText = getString(if (applied) R.string.ack_applied else R.string.ack_failed),
                    applyTimeText = formatNow(),
                    applied = applied,
                    statusText = statusText,
                )
            }
        }
        val configFilter = IntentFilter(HideConfigStore.ACTION_SET_CONFIG_STATUS)
        ContextCompat.registerReceiver(this, configStatusReceiver, configFilter, ContextCompat.RECEIVER_EXPORTED)

        appliedConfigReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: android.content.Context?, intent: Intent?) {
                val statusIntent = intent ?: return
                val token = statusIntent.getStringExtra(HideConfigStore.EXTRA_QUERY_TOKEN) ?: return
                if (!mainViewModel.consumeAppliedConfigResult(token)) {
                    return
                }
                appliedConfigTimeoutRunnable?.let { mainHandler.removeCallbacks(it) }
                appliedConfigTimeoutRunnable = null
                val config = HideConfigStore.fromBundle(statusIntent.extras)
                val snapshotText = if (config == null) {
                    getString(R.string.config_snapshot_missing) + "\n"
                } else {
                    buildAppliedConfigSnapshot(config)
                }
                val stateBeforeRecovery = mainViewModel.uiState.value
                if (stateBeforeRecovery.localConfigMissing && config != null && config != HideConfigDefaults.value) {
                    mainViewModel.recoverConfig(config)
                    saveAndReloadHideConfig(getString(R.string.config_recovered_applied) + "\n")
                }
                val draft = mainViewModel.uiState.value.currentHideConfig
                mainViewModel.applyAppliedConfig(
                    config = config,
                    snapshotText = snapshotText,
                    draftDiffers = hasDraftVsAppliedDifferences(draft, config),
                )
            }
        }
        val appliedConfigFilter = IntentFilter(HideConfigStore.ACTION_SET_APPLIED_HIDE_CONFIG)
        ContextCompat.registerReceiver(this, appliedConfigReceiver, appliedConfigFilter, ContextCompat.RECEIVER_EXPORTED)

        setContent {
            val navigator = rememberNavigator<Route>(Route.Main)
            val uiState by mainViewModel.uiState.collectAsStateWithLifecycle()
            val darkMode = isSystemInDarkTheme()
            DisposableEffect(darkMode) {
                enableEdgeToEdge(
                    statusBarStyle = SystemBarStyle.auto(
                        android.graphics.Color.TRANSPARENT,
                        android.graphics.Color.TRANSPARENT,
                    ) { darkMode },
                    navigationBarStyle = SystemBarStyle.auto(
                        android.graphics.Color.TRANSPARENT,
                        android.graphics.Color.TRANSPARENT,
                    ) { darkMode },
                )
                window.isNavigationBarContrastEnforced = false
                onDispose { }
            }
            CompositionLocalProvider(LocalUiMode provides uiState.uiMode) {
                CompositionLocalProvider(
                    LocalNavigator provides navigator,
                    LocalMiuixBlurEnabled provides uiState.enableMiuixBlur,
                ) {
                    FuseHideTheme {
                        val appListViewModel: AppListViewModel = viewModel()
                        val hookStatusState = remember(
                            uiState.infoText,
                            uiState.statusText,
                            uiState.hookBackend,
                            uiState.hookedPackage,
                            uiState.hookedPid,
                            uiState.hookCheckCompleted,
                        ) {
                            hookStatusUiState(uiState)
                        }
                        val configDiff = remember(
                            uiState.currentHideConfig,
                            uiState.appliedHideConfig,
                        ) {
                            buildDraftVsAppliedDiff(
                                this@MainActivity,
                                uiState.currentHideConfig,
                                uiState.appliedHideConfig,
                            )
                        }
                        val configState = remember(
                            uiState.configStatusText,
                            uiState.lastAckTokenText,
                            uiState.lastAckResultText,
                            uiState.lastApplyTimeText,
                            uiState.appliedConfigSnapshotText,
                            uiState.appliedConfigQueryPending,
                            uiState.appliedHideConfig,
                            uiState.highlightConfigResults,
                            uiState.configResultsScrollToken,
                            uiState.currentHideConfig,
                            configDiff,
                        ) {
                            configUiState(uiState, configDiff)
                        }
                        val debugState = remember(
                            uiState.pathText,
                            uiState.pathText2,
                            uiState.outputText,
                        ) {
                            debugUiState(uiState)
                        }
                        val settingsState = remember(
                            uiState.uiMode,
                            uiState.enableMiuixBlur,
                            uiState.enableMiuixFloatingBottomBar,
                        ) {
                            SettingsUiState(
                                uiMode = uiState.uiMode,
                                enableMiuixBlur = uiState.enableMiuixBlur,
                                enableMiuixFloatingBottomBar = uiState.enableMiuixFloatingBottomBar,
                            )
                        }
                        val homeCallbacks = remember(navigator) {
                            HomeCallbacks(
                                onStatusClick = {
                                    startStatusCheck()
                                    refreshAppliedConfig()
                                },
                                onConfigSyncClick = {
                                    refreshAppliedConfig()
                                    navigator.push(Route.AppliedConfig)
                                },
                            )
                        }
                        val configCallbacks = remember { configCallbacks() }
                        val debugCallbacks = remember { debugCallbacks() }
                        val settingsCallbacks = remember {
                            SettingsCallbacks(
                                onToggleUiMode = {
                                    val current = mainViewModel.uiState.value.uiMode
                                    val next = if (current == UiMode.Miuix) UiMode.Material else UiMode.Miuix
                                    FuseHideUiModeStore.saveToPrefs(this@MainActivity, next)
                                    mainViewModel.setUiMode(next)
                                },
                                onToggleMiuixBlur = {
                                    val enabled = !mainViewModel.uiState.value.enableMiuixBlur
                                    FuseHideUiModeStore.saveMiuixBlurEnabled(this@MainActivity, enabled)
                                    mainViewModel.setMiuixBlurEnabled(enabled)
                                },
                                onToggleMiuixFloatingBottomBar = {
                                    val enabled = !mainViewModel.uiState.value.enableMiuixFloatingBottomBar
                                    FuseHideUiModeStore.saveMiuixFloatingBottomBarEnabled(this@MainActivity, enabled)
                                    mainViewModel.setMiuixFloatingBottomBarEnabled(enabled)
                                },
                            )
                        }
                        val onTabSelected = remember {
                            { index: Int -> mainViewModel.setSelectedTab(index) }
                        }
                        val onOpenGlobalConfig = remember(navigator) {
                            { navigator.push(Route.GlobalConfig) }
                        }
                        val onOpenAppConfig = remember(navigator) {
                            { packageName: String -> navigator.push(Route.AppConfig(packageName)) }
                        }
                        val mainScreenEntry: @Composable () -> Unit = {
                            MainPage(
                                selectedTab = uiState.selectedTab,
                                onTabSelected = onTabSelected,
                                hookStatus = hookStatusState,
                                configState = configState,
                                debugState = debugState,
                                homeCallbacks = homeCallbacks,
                                configCallbacks = configCallbacks,
                                appListViewModel = appListViewModel,
                                onOpenGlobalConfig = onOpenGlobalConfig,
                                onOpenAppConfig = onOpenAppConfig,
                                debugCallbacks = debugCallbacks,
                                settingsState = settingsState,
                                settingsCallbacks = settingsCallbacks,
                            )
                        }

                        val navDisplay: @Composable () -> Unit = {
                            val swipeDismiss = if (
                                LocalLayoutDirection.current == androidx.compose.ui.unit.LayoutDirection.Rtl
                            ) {
                                NavSwipeDirection.RightToLeft
                            } else {
                                NavSwipeDirection.LeftToRight
                            }
                            NavDisplay(
                                backStack = navigator.backStack,
                                effects = NavDisplayEffects(
                                    cornerClipRadius = rememberNavSystemCornerRadius(),
                                ),
                                onBack = { navigator.pop() },
                            ) {
                                entry<Route.Main>(swipeDismiss = swipeDismiss) {
                                    mainScreenEntry()
                                }
                                entry<Route.GlobalConfig>(swipeDismiss = swipeDismiss) {
                                    GlobalConfigPage(
                                        state = configState,
                                        callbacks = configCallbacks,
                                        onBack = { navigator.pop() },
                                        onSave = ::applyHideConfig,
                                    )
                                }
                                entry<Route.AppliedConfig>(swipeDismiss = swipeDismiss) {
                                    io.github.xiaotong6666.fusehide.ui.feature.home.AppliedConfigPage(
                                        snapshotText = configState.appliedConfigSnapshotText,
                                        onBack = { navigator.pop() },
                                    )
                                }
                                entry<Route.AppConfig>(swipeDismiss = swipeDismiss) { key ->
                                    AppConfigPage(
                                        packageName = key.packageName,
                                        state = configState,
                                        callbacks = configCallbacks,
                                        appListViewModel = appListViewModel,
                                        onBack = { navigator.pop() },
                                        onSave = ::applyHideConfig,
                                    )
                                }
                            }
                        }

                        when (uiState.uiMode) {
                            UiMode.Material -> androidx.compose.material3.Scaffold(
                                containerColor = MaterialTheme.colorScheme.surfaceContainer,
                            ) { _ ->
                                Box(modifier = Modifier.fillMaxSize()) {
                                    navDisplay()
                                }
                            }

                            UiMode.Miuix -> top.yukonga.miuix.kmp.basic.Scaffold { navDisplay() }
                        }
                    }
                }
            }
        }

        startStatusCheck()
        refreshAppliedConfig(autoScrollToResults = false)
        handleDebugIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleDebugIntent(intent)
    }

    private fun handleDebugIntent(intent: Intent?) {
        val debugPath = intent?.getStringExtra(EXTRA_DEBUG_PATH)
        if (debugPath.isNullOrEmpty()) {
            return
        }
        val debugPath2 = intent.getStringExtra(EXTRA_DEBUG_PATH2).orEmpty()
        mainViewModel.setDebugPaths(debugPath, debugPath2)
        val debugActions = intent.getStringExtra(EXTRA_DEBUG_ACTIONS)
        Log.d("FuseHide", "handleDebugIntent path=$debugPath path2=$debugPath2 actions=$debugActions")
        appendOutput(
            "ADB debug intent path=${PathDebugText.escapeNonAscii(debugPath)} path2=${PathDebugText.escapeNonAscii(debugPath2)} actions=${debugActions ?: "(default)"}\n",
        )
        window.decorView.postDelayed({ runDebugProbe() }, 1500L)
    }

    private fun runDebugProbe() {
        val actions = intent.getStringExtra(EXTRA_DEBUG_ACTIONS)
            ?.split(',')
            ?.map { it.trim().lowercase() }
            ?.filter { it.isNotEmpty() }
            ?: listOf("stat", "access", "list", "open")
        val state = mainViewModel.uiState.value
        val pathText = state.pathText
        val pathText2 = state.pathText2
        Log.d("FuseHide", "runDebugProbe path=$pathText path2=$pathText2 actions=$actions")
        mainViewModel.clearOutput()
        appendOutput(
            "Running debug probe path=${PathDebugText.escapeNonAscii(pathText)} path2=${PathDebugText.escapeNonAscii(pathText2)} actions=${actions.joinToString(",")}\n",
        )
        actions.forEach { action ->
            when (action) {
                "stat" -> runPathCheck(0)
                "access" -> runPathCheck(1)
                "list" -> runPathCheck(2)
                "open" -> runPathCheck(3)
                "getcon" -> runPathCheck(4)
                "create" -> runPathCheck(5)
                "mkdir" -> runPathCheck(6)
                "move", "rename" -> runPathCheck(7)
                "rmdir" -> runPathCheck(8)
                "unlink" -> runPathCheck(9)
            }
        }
    }

    fun onHookCheckTimeout() {
        statusCheckInFlight = false
        mainViewModel.completeHookStatusCheck(getString(R.string.status_not_hooked) + "\n")
        logUiText(mainViewModel.uiState.value.statusText)
    }

    override fun onHookStatusReceived(packageName: String, pid: Int, backend: String?) {
        statusTimeoutRunnable?.let { mainHandler.removeCallbacks(it) }
        statusTimeoutRunnable = null
        activeStatusCheckToken = null
        statusCheckInFlight = false
        mainViewModel.setHookedStatus(
            packageName = packageName,
            pid = pid,
            backend = when (backend) {
                "xposed" -> HookBackend.Xposed
                "zygisk" -> HookBackend.Zygisk
                else -> null
            },
            statusText = getString(R.string.status_hooked, packageName, pid) + "\n",
        )
        logUiText(mainViewModel.uiState.value.statusText)
    }

    override fun getActiveStatusCheckToken(): String? = activeStatusCheckToken

    private fun buildInfoText(): String {
        val utsname: StructUtsname = Os.uname()
        val marketName = sequenceOf(
            getSystemProperty("ro.product.marketname"),
            getSystemProperty("ro.product.vendor.marketname"),
            getSystemProperty("ro.product.odm.marketname"),
        ).firstOrNull { it.isNotBlank() }
            ?: "${Build.MANUFACTURER.replaceFirstChar { it.titlecase() }} ${Build.MODEL}"
        val deviceText = "$marketName / ${Build.MODEL} (${Build.DEVICE})"
        return buildString {
            append("Version: ${BuildConfig.VERSION_NAME} (${BuildConfig.COMMIT_HASH})\n")
            append("Kernel: ${utsname.release}\n")
            append("Device: $deviceText\n")
            append("System: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})\n")
            if (getBooleanSystemProperty("external_storage.sdcardfs.enabled")) {
                append("sdcardfs: true\n")
            }
            val fuseBpf = getBooleanSystemProperty("ro.fuse.bpf.is_running")
            append("fuse bpf: ${if (fuseBpf) "supported" else "unsupported"}\n")
            val dataIsolation = getBooleanSystemProperty("persist.sys.vold_app_data_isolation_enabled")
            append("AppDataIsolation: ${if (dataIsolation) "enabled" else "disabled"}\n")
            if (!fuseBpf && !dataIsolation) {
                append("App data isolation is required to fix Android/data access.\n")
                append("Use `setprop persist.sys.vold_app_data_isolation_enabled 1` to enable it.\n")
            }
        }
    }

    private fun startStatusCheck() {
        if (statusCheckInFlight) {
            Log.d("FuseHide", "status check already in flight, ignore duplicate request")
            return
        }

        val requestToken = UUID.randomUUID().toString()
        activeStatusCheckToken = requestToken
        statusCheckInFlight = true
        val statusText = getString(R.string.status_checking) + "\n"
        mainViewModel.beginHookStatusCheck(statusText)
        logUiText(statusText)
        hookStatusProbe.start(requestToken)
    }

    private fun runPathCheck(mode: Int) {
        val state = mainViewModel.uiState.value
        appendOutput(PathDebugActions.runPathCheck(mode, state.pathText, state.pathText2))
    }

    private fun runAllPkgCheck() {
        mainViewModel.setOutput("Scanning all packages... (this may take a while)\n")
        val path = mainViewModel.uiState.value.pathText
        lifecycleScope.launch(Dispatchers.IO) {
            val output = PathDebugActions.runAllPkgCheck(packageManager, path)
            runOnUiThread {
                appendOutput(output)
            }
        }
    }

    private fun insertZwj() {
        mainViewModel.appendZeroWidthJoiner()
    }

    private fun copyAll() {
        val clipboardManager = getSystemService(ClipboardManager::class.java) ?: return
        val state = mainViewModel.uiState.value
        val allText = buildString {
            append("Info:\n")
            append(state.infoText)
            append("\nStatus:\n")
            append(state.statusText)
            append("\nTest:\n")
            append(state.outputText)
        }
        clipboardManager.setPrimaryClip(ClipData.newPlainText("", allText))
    }

    private fun hookStatusUiState(state: MainActivityUiState): HookStatusUiState = HookStatusUiState(
        infoText = state.infoText,
        statusText = state.statusText,
        isHooked = state.hookedPackage != null,
        backend = state.hookBackend,
        hookedPackage = state.hookedPackage,
        hookedPid = state.hookedPid,
        hookCheckCompleted = state.hookCheckCompleted,
    )

    private fun configUiState(
        state: MainActivityUiState,
        diff: io.github.xiaotong6666.fusehide.ui.core.model.HideConfigDiff,
    ): ConfigUiState = ConfigUiState(
        configStatusText = state.configStatusText,
        lastAckTokenText = state.lastAckTokenText,
        lastAckResultText = state.lastAckResultText,
        lastApplyTimeText = state.lastApplyTimeText,
        draftVsAppliedDiff = diff,
        appliedConfigSnapshotText = state.appliedConfigSnapshotText,
        appliedConfigQueryPending = state.appliedConfigQueryPending,
        hasAppliedConfig = state.appliedHideConfig != null,
        highlightConfigResults = state.highlightConfigResults,
        configResultsScrollToken = state.configResultsScrollToken,
        currentHideConfig = state.currentHideConfig,
    )

    private fun debugUiState(state: MainActivityUiState): DebugUiState = DebugUiState(
        pathText = state.pathText,
        pathText2 = state.pathText2,
        outputText = state.outputText,
    )

    private fun configCallbacks(): ConfigCallbacks = ConfigCallbacks(
        onStatusClick = ::startStatusCheck,
        onConfigUpdate = mainViewModel::updateConfig,
        onApplyConfigClick = ::applyHideConfig,
        onResetConfigClick = ::resetHideConfigToDefaults,
    )

    private fun debugCallbacks(): DebugCallbacks = DebugCallbacks(
        onStatusClick = ::startStatusCheck,
        onPathChanged = mainViewModel::setPathText,
        onPath2Changed = mainViewModel::setPathText2,
        onStatClick = { runPathCheck(0) },
        onAccessClick = { runPathCheck(1) },
        onListClick = { runPathCheck(2) },
        onOpenClick = { runPathCheck(3) },
        onGetConClick = { runPathCheck(4) },
        onCreateClick = { runPathCheck(5) },
        onMkdirClick = { runPathCheck(6) },
        onMoveClick = { runPathCheck(7) },
        onRmdirClick = { runPathCheck(8) },
        onUnlinkClick = { runPathCheck(9) },
        onAllPkgClick = ::runAllPkgCheck,
        onInsertZwjClick = ::insertZwj,
        onClearClick = mainViewModel::clearOutput,
        onResetClick = mainViewModel::resetDebugPath,
        onCopyAllClick = ::copyAll,
        onSelfDataClick = { appendOutput("external files dir: ${getExternalFilesDir("")}\n") },
    )

    private fun applyHideConfig() {
        saveAndReloadHideConfig(getString(R.string.config_waiting_ack) + "\n")
    }

    private fun saveAndReloadHideConfig(statusMessage: String) {
        if (refuseMissingLocalDefaultSave()) {
            return
        }
        val reloadToken = mainViewModel.beginConfigReload(statusMessage)
        HideConfigStore.save(this, mainViewModel.uiState.value.currentHideConfig, reloadToken)
        HideConfigStore.sendReloadBroadcast(this, reloadToken)
        startStatusCheck()
    }

    private fun refreshAppliedConfig(autoScrollToResults: Boolean = false) {
        appliedConfigTimeoutRunnable?.let { mainHandler.removeCallbacks(it) }
        val queryToken = mainViewModel.beginAppliedConfigQuery(
            snapshotText = getString(R.string.config_snapshot_waiting) + "\n",
            autoScrollToResults = autoScrollToResults,
        )
        HideConfigStore.sendAppliedConfigQueryBroadcast(this, queryToken)
        val timeoutRunnable = Runnable {
            if (mainViewModel.timeoutAppliedConfigQuery(
                    queryToken,
                    getString(R.string.config_snapshot_missing) + "\n",
                )
            ) {
                appliedConfigTimeoutRunnable = null
            }
        }
        appliedConfigTimeoutRunnable = timeoutRunnable
        mainHandler.postDelayed(timeoutRunnable, APPLIED_CONFIG_QUERY_TIMEOUT_MS)
    }

    private fun resetHideConfigToDefaults() {
        mainViewModel.markConfigRestoredDefaults(getString(R.string.config_restored_defaults) + "\n")
    }

    private fun refuseMissingLocalDefaultSave(): Boolean {
        val state = mainViewModel.uiState.value
        if (!state.localConfigMissing ||
            state.currentHideConfig != HideConfigDefaults.value ||
            state.defaultSaveExplicitlyRequested
        ) {
            return false
        }
        mainViewModel.markDefaultOverwriteRefused(getString(R.string.config_refuse_default_overwrite) + "\n")
        return true
    }

    private fun appendOutput(text: String) {
        mainViewModel.appendOutput(text)
        logUiText(text)
    }

    private fun logUiText(text: String) {
        text.lineSequence()
            .map { it.trimEnd() }
            .filter { it.isNotEmpty() }
            .forEach { Log.i("FuseHide", it) }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(statusReceiver)
        unregisterReceiver(configStatusReceiver)
        unregisterReceiver(appliedConfigReceiver)
        statusTimeoutRunnable?.let { mainHandler.removeCallbacks(it) }
        appliedConfigTimeoutRunnable?.let { mainHandler.removeCallbacks(it) }
    }
}
