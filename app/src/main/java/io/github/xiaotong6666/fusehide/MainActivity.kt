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

package io.github.xiaotong6666.fusehide

import android.content.BroadcastReceiver
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Intent
import android.content.IntentFilter
import android.os.Binder
import android.os.Build
import android.os.Bundle
import android.system.Os
import android.system.StructUtsname
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import io.github.xiaotong6666.fusehide.ui.ConfigCallbacks
import io.github.xiaotong6666.fusehide.ui.ConfigUiState
import io.github.xiaotong6666.fusehide.ui.DebugCallbacks
import io.github.xiaotong6666.fusehide.ui.DebugUiState
import io.github.xiaotong6666.fusehide.ui.FuseHideHomeScreen
import io.github.xiaotong6666.fusehide.ui.HookStatusUiState
import io.github.xiaotong6666.fusehide.ui.theme.fuseHideTheme
import java.lang.ref.WeakReference
import java.util.UUID

class MainActivity : ComponentActivity() {
    companion object {
        private const val EXTRA_DEBUG_PATH = "debug_path"
        private const val EXTRA_DEBUG_ACTIONS = "debug_actions"

        fun getBooleanSystemProperty(name: String): Boolean = try {
            Class.forName("android.os.SystemProperties")
                .getDeclaredMethod("getBoolean", String::class.java, Boolean::class.javaPrimitiveType)
                .invoke(null, name, false) as Boolean
        } catch (th: Throwable) {
            Log.e("FuseHide", "getProp", th)
            false
        }
    }

    private var infoText by mutableStateOf("")
    private var statusText by mutableStateOf("")
    private var selectedTab by mutableStateOf(0)
    private var configStatusText by mutableStateOf("")
    private var lastAckTokenText by mutableStateOf("-")
    private var lastAckResultText by mutableStateOf("-")
    private var lastApplyTimeText by mutableStateOf("-")
    private var appliedHideConfig: HideConfig? by mutableStateOf(null)
    private var appliedConfigSnapshotText by mutableStateOf("")
    private var highlightConfigResults by mutableStateOf(false)
    private var configResultsScrollToken by mutableStateOf(0)
    private var shouldAutoScrollConfigResults by mutableStateOf(false)
    private var enableHideAllRootEntries by mutableStateOf(HideConfigDefaults.value.enableHideAllRootEntries)
    private var hideAllRootEntriesExemptionsText by mutableStateOf(
        HideConfigDefaults.toEditorText(HideConfigDefaults.value.hideAllRootEntriesExemptions),
    )
    private var hiddenTargetsText by mutableStateOf(
        HideConfigDefaults.toEditorText(
            HideConfigDefaults.value.hiddenRootEntryNames + HideConfigDefaults.value.hiddenRelativePaths,
        ),
    )
    private var hiddenPackagesText by mutableStateOf(
        HideConfigDefaults.toEditorText(HideConfigDefaults.value.hiddenPackages),
    )
    private var pathText by mutableStateOf(PathDebugActions.defaultPath())
    private var pathText2 by mutableStateOf("")
    private var outputText by mutableStateOf("")

    private var hookedPackage: String? = null
    private var hookedPid: Int = -1
    private var statusBinderReference: WeakReference<Binder>? = null
    private var hookCheckCompleted: Boolean = false
    private var statusCheckInFlight: Boolean = false
    private var statusCheckThread: Thread? = null
    private lateinit var statusReceiver: StatusBroadcastReceiver
    private lateinit var configStatusReceiver: BroadcastReceiver
    private lateinit var appliedConfigReceiver: BroadcastReceiver
    private var pendingReloadToken: String? = null
    private var pendingQueryToken: String? = null
    private val hookStatusProbe by lazy {
        HookStatusProbe(
            context = this,
            onTimeout = {
                statusBinderReference = null
                onHookCheckTimeout()
            },
            onStarted = { binderReference, statusThread ->
                statusBinderReference = binderReference
                statusCheckThread = statusThread
            },
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        appendInfo()
        applyConfigToEditor(HideConfigStore.load(this))
        configStatusText = getString(R.string.config_loaded_saved) + "\n"
        appliedConfigSnapshotText = getString(R.string.config_snapshot_missing) + "\n"

        statusReceiver = StatusBroadcastReceiver(this, 1)
        val filter = IntentFilter(HookStatusProbe.ACTION_SET_STATUS)
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(statusReceiver, filter, HookStatusProbe.registerReceiverFlags())
        } else {
            registerReceiver(statusReceiver, filter)
        }

        configStatusReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: android.content.Context?, intent: Intent?) {
                val token = intent?.getStringExtra(HideConfigStore.EXTRA_RELOAD_TOKEN)
                if (token == null || token != pendingReloadToken) {
                    return
                }
                pendingReloadToken = null
                val applied = intent.getBooleanExtra(HideConfigStore.EXTRA_RELOAD_APPLIED, false)
                val message = intent.getStringExtra(HideConfigStore.EXTRA_RELOAD_MESSAGE) ?: "unknown"
                lastAckTokenText = token
                lastAckResultText = getString(if (applied) R.string.ack_applied else R.string.ack_failed)
                lastApplyTimeText = formatNow()
                highlightConfigResults = !applied
                configStatusText = if (applied) {
                    refreshAppliedConfig(autoScrollToResults = true)
                    getString(R.string.config_applied_ok) + "\n"
                } else {
                    getString(R.string.config_applied_fail, message) + "\n"
                }
                if (!applied) {
                    shouldAutoScrollConfigResults = true
                    configResultsScrollToken += 1
                }
            }
        }
        val configFilter = IntentFilter(HideConfigStore.ACTION_SET_CONFIG_STATUS)
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(configStatusReceiver, configFilter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(configStatusReceiver, configFilter)
        }

        appliedConfigReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: android.content.Context?, intent: Intent?) {
                val token = intent?.getStringExtra(HideConfigStore.EXTRA_QUERY_TOKEN)
                if (token == null || token != pendingQueryToken) {
                    return
                }
                pendingQueryToken = null
                val config = HideConfigStore.fromBundle(intent.extras)
                appliedHideConfig = config
                appliedConfigSnapshotText = if (config == null) {
                    getString(R.string.config_snapshot_missing) + "\n"
                } else {
                    buildAppliedConfigSnapshot(config)
                }
                highlightConfigResults = config == null || buildDraftVsAppliedDiff(this@MainActivity, currentHideConfig(), config).hasDifferences
                if (shouldAutoScrollConfigResults) {
                    configResultsScrollToken += 1
                    shouldAutoScrollConfigResults = false
                }
            }
        }
        val appliedConfigFilter = IntentFilter(HideConfigStore.ACTION_SET_APPLIED_HIDE_CONFIG)
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(appliedConfigReceiver, appliedConfigFilter, RECEIVER_EXPORTED)
        } else {
            registerReceiver(appliedConfigReceiver, appliedConfigFilter)
        }

        setContent {
            fuseHideTheme {
                FuseHideHomeScreen(
                    selectedTab = selectedTab,
                    onTabSelected = { selectedTab = it },
                    hookStatus = hookStatusUiState(),
                    configState = configUiState(),
                    debugState = debugUiState(),
                    configCallbacks = configCallbacks(),
                    debugCallbacks = debugCallbacks(),
                )
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
        selectedTab = 1
        pathText = debugPath
        val debugActions = intent.getStringExtra(EXTRA_DEBUG_ACTIONS)
        Log.d("FuseHide", "handleDebugIntent path=$debugPath actions=$debugActions")
        appendOutput("ADB debug intent path=${PathDebugText.escapeNonAscii(debugPath)} actions=${debugActions ?: "(default)"}\n")
        window.decorView.postDelayed({ runDebugProbe() }, 1500L)
    }

    private fun runDebugProbe() {
        val actions = intent.getStringExtra(EXTRA_DEBUG_ACTIONS)
            ?.split(',')
            ?.map { it.trim().lowercase() }
            ?.filter { it.isNotEmpty() }
            ?: listOf("stat", "access", "list", "open")
        Log.d("FuseHide", "runDebugProbe path=$pathText actions=$actions")
        outputText = ""
        appendOutput("Running debug probe path=${PathDebugText.escapeNonAscii(pathText)} actions=${actions.joinToString(",")}\n")
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
        hookCheckCompleted = true
        updateStatusText()
    }

    fun onHookStatusReceived(packageName: String, pid: Int) {
        hookedPackage = packageName
        hookedPid = pid
        statusCheckThread?.interrupt()
        hookCheckCompleted = true
        updateStatusText()
    }

    private fun appendInfo() {
        val utsname: StructUtsname = Os.uname()
        val sdk = if (Build.VERSION.SDK_INT < 36) Build.VERSION.SDK_INT * 100000 else Build.VERSION.SDK_INT_FULL
        buildString {
            append("FuseHide\n")
            append("Kernel: ${utsname.release}\n")
            append("Release: ${Build.VERSION.RELEASE}\n")
            append("Device: ${Build.DEVICE}\n")
            append("SDK: $sdk\n")
            if (getBooleanSystemProperty("external_storage.sdcardfs.enabled")) {
                append("sdcardfs=true\n")
            }
            val fuseBpf = getBooleanSystemProperty("ro.fuse.bpf.is_running")
            append("fuse bpf: ${if (fuseBpf) "supported" else "unsupported"}\n")
            val dataIsolation = getBooleanSystemProperty("persist.sys.vold_app_data_isolation_enabled")
            append("AppDataIsolation: ${if (dataIsolation) "enabled" else "disabled"}\n")
            if (!fuseBpf && !dataIsolation) {
                append("App data isolation is required to fix Android/data access.\n")
                append("Use `setprop persist.sys.vold_app_data_isolation_enabled 1` to enable it.\n")
            }
        }.also { infoText = it }
    }

    private fun startStatusCheck() {
        if (statusCheckInFlight) return

        hookedPackage = null
        hookedPid = -1
        hookCheckCompleted = false
        updateStatusText()
        statusCheckInFlight = true
        hookStatusProbe.start()
    }

    fun updateStatusText() {
        statusCheckInFlight = false
        statusCheckThread = null
        Log.d("FuseHide", "updateStatusText hookedPackage=$hookedPackage hookCheckCompleted=$hookCheckCompleted pid=$hookedPid")
        statusText = when {
            hookedPackage != null -> getString(R.string.status_hooked, hookedPackage, hookedPid) + "\n"
            hookCheckCompleted -> getString(R.string.status_not_hooked) + "\n"
            else -> getString(R.string.status_checking) + "\n"
        }
        logUiText(statusText)
    }

    private fun runPathCheck(mode: Int) {
        appendOutput(PathDebugActions.runPathCheck(mode, pathText, pathText2))
    }

    private fun runAllPkgCheck() {
        outputText = "Scanning all packages... (this may take a while)\n"
        Thread {
            val output = PathDebugActions.runAllPkgCheck(packageManager, pathText)
            runOnUiThread {
                appendOutput(output)
            }
        }.start()
    }

    private fun insertZwj() {
        pathText += "\\u200d"
    }

    private fun copyAll() {
        val clipboardManager = getSystemService(ClipboardManager::class.java) ?: return
        val allText = buildString {
            append("Info:\n")
            append(infoText)
            append("\nStatus:\n")
            append(statusText)
            append("\nTest:\n")
            append(outputText)
        }
        clipboardManager.setPrimaryClip(ClipData.newPlainText("", allText))
    }

    private fun applyConfigToEditor(config: HideConfig) {
        enableHideAllRootEntries = config.enableHideAllRootEntries
        hideAllRootEntriesExemptionsText = HideConfigDefaults.toEditorText(config.hideAllRootEntriesExemptions)
        hiddenTargetsText = HideConfigDefaults.toEditorText(config.hiddenRootEntryNames + config.hiddenRelativePaths)
        hiddenPackagesText = HideConfigDefaults.toEditorText(config.hiddenPackages)
    }

    private fun currentHideConfig(): HideConfig {
        val (rootNames, relativePaths) = parseHiddenTargetRules(hiddenTargetsText)
        return HideConfig(
            enableHideAllRootEntries = enableHideAllRootEntries,
            hideAllRootEntriesExemptions = HideConfigDefaults.parseEditorText(hideAllRootEntriesExemptionsText),
            hiddenRootEntryNames = rootNames,
            hiddenRelativePaths = relativePaths,
            hiddenPackages = HideConfigDefaults.parseEditorText(hiddenPackagesText),
        )
    }

    private fun hookStatusUiState(): HookStatusUiState = HookStatusUiState(
        infoText = infoText,
        statusText = statusText,
        isHooked = hookedPackage != null,
        hookedPackage = hookedPackage,
        hookedPid = hookedPid,
        hookCheckCompleted = hookCheckCompleted,
    )

    private fun configUiState(): ConfigUiState = ConfigUiState(
        configStatusText = configStatusText,
        lastAckTokenText = lastAckTokenText,
        lastAckResultText = lastAckResultText,
        lastApplyTimeText = lastApplyTimeText,
        draftVsAppliedDiff = buildDraftVsAppliedDiff(this, currentHideConfig(), appliedHideConfig),
        appliedConfigSnapshotText = appliedConfigSnapshotText,
        highlightConfigResults = highlightConfigResults,
        configResultsScrollToken = configResultsScrollToken,
        enableHideAllRootEntries = enableHideAllRootEntries,
        hideAllRootEntriesExemptionsText = hideAllRootEntriesExemptionsText,
        hiddenTargetsText = hiddenTargetsText,
        hiddenPackagesText = hiddenPackagesText,
    )

    private fun debugUiState(): DebugUiState = DebugUiState(
        pathText = pathText,
        pathText2 = pathText2,
        outputText = outputText,
    )

    private fun configCallbacks(): ConfigCallbacks = ConfigCallbacks(
        onStatusClick = ::startStatusCheck,
        onEnableHideAllRootEntriesChanged = { enableHideAllRootEntries = it },
        onHideAllRootEntriesExemptionsChanged = { hideAllRootEntriesExemptionsText = it },
        onHiddenTargetsChanged = { hiddenTargetsText = it },
        onHiddenPackagesChanged = { hiddenPackagesText = it },
        onSaveConfigClick = ::saveHideConfig,
        onApplyConfigClick = ::applyHideConfig,
        onResetConfigClick = ::resetHideConfigToDefaults,
        onRefreshAppliedConfigClick = ::refreshAppliedConfig,
    )

    private fun debugCallbacks(): DebugCallbacks = DebugCallbacks(
        onStatusClick = ::startStatusCheck,
        onPathChanged = { pathText = it },
        onPath2Changed = { pathText2 = it },
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
        onClearClick = { outputText = "" },
        onResetClick = { pathText = PathDebugActions.defaultPath() },
        onCopyAllClick = ::copyAll,
        onSelfDataClick = { appendOutput("external files dir: ${getExternalFilesDir("")}\n") },
    )

    private fun saveHideConfig() {
        HideConfigStore.save(this, currentHideConfig())
        configStatusText = getString(R.string.config_saved_local) + "\n"
    }

    private fun applyHideConfig() {
        val reloadToken = UUID.randomUUID().toString()
        pendingReloadToken = reloadToken
        HideConfigStore.save(this, currentHideConfig(), reloadToken)
        HideConfigStore.sendReloadBroadcast(this, reloadToken)
        configStatusText = getString(R.string.config_waiting_ack) + "\n"
        startStatusCheck()
    }

    private fun refreshAppliedConfig(autoScrollToResults: Boolean = false) {
        val queryToken = UUID.randomUUID().toString()
        pendingQueryToken = queryToken
        shouldAutoScrollConfigResults = autoScrollToResults
        appliedConfigSnapshotText = getString(R.string.config_snapshot_waiting) + "\n"
        HideConfigStore.sendAppliedConfigQueryBroadcast(this, queryToken)
    }

    private fun resetHideConfigToDefaults() {
        applyConfigToEditor(HideConfigDefaults.value)
        configStatusText = getString(R.string.config_restored_defaults) + "\n"
    }

    private fun appendOutput(text: String) {
        outputText += text
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
        statusCheckThread?.interrupt()
    }
}
