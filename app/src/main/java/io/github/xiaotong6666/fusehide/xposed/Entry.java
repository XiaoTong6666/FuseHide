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

package io.github.xiaotong6666.fusehide.xposed;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import androidx.core.content.ContextCompat;
import io.github.libxposed.api.XposedInterface;
import io.github.libxposed.api.XposedModule;
import io.github.libxposed.api.XposedModuleInterface.HotReloadedParam;
import io.github.libxposed.api.XposedModuleInterface.HotReloadingParam;
import io.github.libxposed.api.XposedModuleInterface.ModuleLoadedParam;
import io.github.libxposed.api.XposedModuleInterface.PackageLoadedParam;
import io.github.xiaotong6666.fusehide.config.HideConfig;
import io.github.xiaotong6666.fusehide.config.HideConfigNativeBridge;
import io.github.xiaotong6666.fusehide.config.HideConfigStore;
import io.github.xiaotong6666.fusehide.config.PackageHideRule;
import io.github.xiaotong6666.fusehide.status.StatusBroadcastReceiver;
import java.io.File;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicLong;

public class Entry extends XposedModule {
    private static final String APP_PACKAGE = "io.github.xiaotong6666.fusehide";
    private static final String ACTION_GET_STATUS = APP_PACKAGE + ".GET_STATUS";
    private static final String PACKAGE_MEDIA = "com.android.providers.media.module";
    private static final String PACKAGE_MEDIA_GOOGLE = "com.google.android.providers.media.module";
    private static final long CONFIG_RETRY_DELAY_MS = 15000L;
    private static final int CONFIG_MAX_RETRIES = 8;
    private static final long PROP_RT_HOT_RELOAD = 1L << 3;
    private static final String HOOK_ID_APPLICATION_ATTACH = "entry.application_attach";
    private static final String HOOK_ID_DATA_OBB_DEBUG = "entry.data_obb_debug";
    private static final String NATIVE_PAYLOAD_LIBRARY = "libfusehide_payload.so";
    private static final String STATE_HOOKED_APPLICATION = "hookedApplication";
    private static final String STATE_TARGET_CLASS_LOADER = "targetClassLoader";
    private static final String STATE_TARGET_APPLICATION_INFO = "targetApplicationInfo";
    private static final String STATE_TARGET_PACKAGE_NAME = "targetPackageName";
    private static final String STATE_CONFIG_COMPLETED = "configCompleted";
    private static final String STATE_PENDING_RETRY_COUNT = "pendingRetryCount";
    private static final String STATE_SHOULD_INSTALL_DEBUG_HOOK = "shouldInstallDebugHook";
    private static final AtomicLong CONFIG_RELOAD_EPOCH = new AtomicLong(1L);

    private volatile Handler mainHandler;
    private volatile Application hookedApplication;
    private volatile ClassLoader targetPackageClassLoader;
    private volatile ApplicationInfo targetApplicationInfo;
    private volatile String targetPackageName;
    private volatile XposedInterface.HookHandle applicationAttachHookHandle;
    private volatile XposedInterface.HookHandle dataOrObbDebugHookHandle;
    private volatile StatusBroadcastReceiver statusReceiver;
    private volatile BroadcastReceiver configReceiver;
    private volatile BroadcastReceiver queryReceiver;
    private volatile BroadcastReceiver systemStateReceiver;
    private boolean configLoadCompleted;
    private boolean configLoadInFlight;
    private int pendingConfigRetryCount;
    private boolean shouldInstallDebugHook;
    private final Runnable configRetryRunnable = new Runnable() {
        @Override
        public void run() {
            startConfigReload("delayed_retry_" + pendingConfigRetryCount);
        }
    };

    private Handler getMainHandler() {
        Handler h = mainHandler;
        if (h == null) {
            synchronized (this) {
                h = mainHandler;
                if (h == null) {
                    h = new Handler(Looper.getMainLooper());
                    mainHandler = h;
                }
            }
        }
        return h;
    }

    private static java.util.List<String> splitRuleLines(String value) {
        if (value == null || value.isEmpty()) {
            return java.util.Collections.emptyList();
        }
        final java.util.ArrayList<String> out = new java.util.ArrayList<>();
        for (String line : value.split("\\n")) {
            final String trimmed = line.trim();
            if (!trimmed.isEmpty()) {
                out.add(trimmed);
            }
        }
        return out;
    }

    private static HideConfig currentNativeHideConfig() {
        final String[] rulePackages = HideConfigNativeBridge.getCurrentPackageRulePackages();
        final String[] ruleRoots = HideConfigNativeBridge.getCurrentPackageRuleHiddenRootEntryNames();
        final String[] ruleRelatives = HideConfigNativeBridge.getCurrentPackageRuleHiddenRelativePaths();
        final java.util.ArrayList<PackageHideRule> packageRules = new java.util.ArrayList<>();
        for (int i = 0; i < rulePackages.length; i++) {
            final java.util.List<String> roots =
                    i < ruleRoots.length ? splitRuleLines(ruleRoots[i]) : java.util.Collections.emptyList();
            final java.util.List<String> relatives =
                    i < ruleRelatives.length ? splitRuleLines(ruleRelatives[i]) : java.util.Collections.emptyList();
            packageRules.add(new PackageHideRule(rulePackages[i], roots, relatives));
        }
        return new HideConfig(
                HideConfigNativeBridge.getCurrentEnableHideAllRootEntries(),
                java.util.Arrays.asList(HideConfigNativeBridge.getCurrentHideAllRootEntriesExemptions()),
                java.util.Arrays.asList(HideConfigNativeBridge.getCurrentHiddenRootEntryNames()),
                java.util.Arrays.asList(HideConfigNativeBridge.getCurrentHiddenRelativePaths()),
                java.util.Arrays.asList(HideConfigNativeBridge.getCurrentHiddenPackages()),
                packageRules);
    }

    private static void sendConfigStatus(
            Application application, String requestedToken, boolean applied, String message) {
        application.sendBroadcast(new Intent(HideConfigStore.ACTION_SET_CONFIG_STATUS)
                .setPackage(APP_PACKAGE)
                .putExtra(HideConfigStore.EXTRA_RELOAD_TOKEN, requestedToken)
                .putExtra(HideConfigStore.EXTRA_RELOAD_APPLIED, applied)
                .putExtra(HideConfigStore.EXTRA_RELOAD_MESSAGE, message));
    }

    private static void finishConfigReload(
            Application application,
            String requestedToken,
            android.os.Bundle bundle,
            String source,
            BroadcastReceiver.PendingResult pendingResult) {
        try {
            final HideConfig config = HideConfigStore.fromBundle(bundle);
            final String bundleToken = HideConfigStore.reloadTokenFromBundle(bundle);
            final boolean tokenMatches = requestedToken != null && requestedToken.equals(bundleToken);
            boolean applied = false;
            String message;
            if (bundle == null || config == null) {
                message = "hide config unavailable";
            } else if (!tokenMatches) {
                message = "reload token mismatch";
            } else {
                applied = HideConfigStore.applyBundleToNative(bundle);
                if (applied) {
                    HideConfigStore.saveInjectedProcessSnapshot(application, config, bundleToken);
                    message = "hide config applied";
                } else {
                    message = "apply failed";
                }
            }
            sendConfigStatus(application, requestedToken, applied, message);
            Log.d(
                    "FuseHide",
                    "config reload source=" + source + " applied=" + applied + " tokenMatches=" + tokenMatches);
        } finally {
            pendingResult.finish();
        }
    }

    private void onConfigReloadFinished(String source, boolean applied) {
        configLoadInFlight = false;
        if (applied) {
            configLoadCompleted = true;
            pendingConfigRetryCount = 0;
            getMainHandler().removeCallbacks(configRetryRunnable);
            Log.d("FuseHide", "config reload source=" + source + " applied=true");
            return;
        }
        Log.d("FuseHide", "config reload source=" + source + " applied=false");
        scheduleConfigRetry(source);
    }

    private void scheduleConfigRetry(String source) {
        if (hookedApplication == null || configLoadCompleted) {
            return;
        }
        if (pendingConfigRetryCount >= CONFIG_MAX_RETRIES) {
            Log.w("FuseHide", "config retry exhausted source=" + source);
            return;
        }
        pendingConfigRetryCount += 1;
        getMainHandler().removeCallbacks(configRetryRunnable);
        getMainHandler().postDelayed(configRetryRunnable, CONFIG_RETRY_DELAY_MS);
        Log.d(
                "FuseHide",
                "scheduled config retry source="
                        + source
                        + " attempt="
                        + pendingConfigRetryCount
                        + " delayMs="
                        + CONFIG_RETRY_DELAY_MS);
    }

    private void startConfigReload(String source) {
        final Application application = hookedApplication;
        if (application == null || configLoadCompleted || configLoadInFlight) {
            return;
        }
        configLoadInFlight = true;
        final long reloadEpoch = CONFIG_RELOAD_EPOCH.get();
        HideConfigStore.reloadInjectedProcessConfig(
                application,
                applied -> {
                    if (CONFIG_RELOAD_EPOCH.get() != reloadEpoch) {
                        Log.d("FuseHide", "ignore stale config reload callback source=" + source);
                        return;
                    }
                    onConfigReloadFinished(source, applied);
                },
                () -> CONFIG_RELOAD_EPOCH.get() == reloadEpoch);
    }

    private boolean supportsRuntimeHotReload() {
        return (getFrameworkProperties() & PROP_RT_HOT_RELOAD) != 0;
    }

    private long currentModuleVersionCode() {
        final String sourceDir = getModuleApplicationInfo().sourceDir;
        if (sourceDir == null || sourceDir.isEmpty()) {
            return 0L;
        }
        return new File(sourceDir).lastModified();
    }

    private String currentModuleVersionHash() {
        final String sourceDir = getModuleApplicationInfo().sourceDir;
        return sourceDir != null ? sourceDir : "";
    }

    private String currentExternalPayloadPath() {
        final ApplicationInfo applicationInfo = getModuleApplicationInfo();
        final String nativeLibraryDir = applicationInfo.nativeLibraryDir;
        if (nativeLibraryDir == null || nativeLibraryDir.isEmpty()) {
            return null;
        }
        return new File(nativeLibraryDir, NATIVE_PAYLOAD_LIBRARY).getAbsolutePath();
    }

    private void advanceNativeGeneration(String source, boolean preferExternalPayload) {
        final long versionCode = currentModuleVersionCode();
        final String versionHash = currentModuleVersionHash();
        final long previousGeneration = HideConfigNativeBridge.getCurrentNativeGenerationId();
        long nextGeneration = 0L;
        String generationKind = "builtin";
        String payloadPath = null;
        if (preferExternalPayload) {
            payloadPath = currentExternalPayloadPath();
            if (payloadPath != null && new File(payloadPath).isFile()) {
                nextGeneration =
                        HideConfigNativeBridge.switchToExternalNativeGeneration(payloadPath, versionCode, versionHash);
                if (nextGeneration != 0L) {
                    generationKind = "external";
                }
            }
        }
        if (nextGeneration == 0L) {
            nextGeneration = HideConfigNativeBridge.switchToBuiltinNativeGeneration(versionCode, versionHash);
            generationKind = "builtin";
        }
        if (nextGeneration != 0 && nextGeneration != previousGeneration) {
            Log.d(
                    "FuseHide",
                    "advanced native generation source="
                            + source
                            + " kind="
                            + generationKind
                            + " runtimeHotReload="
                            + supportsRuntimeHotReload()
                            + " payloadPath="
                            + payloadPath
                            + " generation="
                            + previousGeneration
                            + "->"
                            + nextGeneration);
        }
    }

    private XposedInterface.HookBuilder hookBuilderWithId(Executable executable, String hookId) {
        return hook(executable).setId(hookId);
    }

    private XposedInterface.Hooker createDataOrObbDebugHooker() {
        return chain -> {
            Object result = chain.proceed();
            Log.d(
                    "FuseHide",
                    "isUidAllowedAccessToDataOrObbPathForFuse uid="
                            + chain.getArgs().get(0)
                            + " path="
                            + chain.getArgs().get(1)
                            + " result="
                            + result);
            return result;
        };
    }

    private XposedInterface.Hooker createApplicationAttachHooker() {
        return chain -> {
            Object result = chain.proceed();
            if (hookedApplication == null) {
                Application app = (Application) chain.getThisObject();
                hookedApplication = app;
                Log.d("FuseHide", "captured Application via attach hook");
                new Handler(Looper.getMainLooper()).post(new MainThreadTask(0, this));
            }
            return result;
        };
    }

    private Method resolveDebugHookTargetMethod(ClassLoader classLoader) throws Throwable {
        Class<?> mediaProviderClass = classLoader.loadClass("com.android.providers.media.MediaProvider");
        return mediaProviderClass.getMethod("isUidAllowedAccessToDataOrObbPathForFuse", int.class, String.class);
    }

    private XposedInterface.HookHandle installDebugHookIfNeeded(
            ClassLoader classLoader, ApplicationInfo applicationInfo) {
        shouldInstallDebugHook = (applicationInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
        if (!shouldInstallDebugHook) {
            dataOrObbDebugHookHandle = null;
            return null;
        }
        try {
            Method targetMethod = resolveDebugHookTargetMethod(classLoader);
            dataOrObbDebugHookHandle =
                    hookBuilderWithId(targetMethod, HOOK_ID_DATA_OBB_DEBUG).intercept(createDataOrObbDebugHooker());
            return dataOrObbDebugHookHandle;
        } catch (Throwable th) {
            Log.e("FuseHide", "hook isUidAllowedAccessToDataOrObbPathForFuse", th);
            dataOrObbDebugHookHandle = null;
            return null;
        }
    }

    private XposedInterface.HookHandle replaceOrInstallApplicationAttachHook(XposedInterface.HookHandle oldHandle)
            throws Throwable {
        Method attachMethod = Application.class.getDeclaredMethod("attach", Context.class);
        if (oldHandle != null) {
            return oldHandle.replaceHook(createApplicationAttachHooker());
        }
        return hookBuilderWithId(attachMethod, HOOK_ID_APPLICATION_ATTACH).intercept(createApplicationAttachHooker());
    }

    private XposedInterface.HookHandle replaceOrInstallDebugHook(XposedInterface.HookHandle oldHandle)
            throws Throwable {
        if (!shouldInstallDebugHook || targetPackageClassLoader == null || targetApplicationInfo == null) {
            if (oldHandle != null) {
                oldHandle.unhook();
            }
            return null;
        }
        if (oldHandle != null) {
            return oldHandle.replaceHook(createDataOrObbDebugHooker());
        }
        return installDebugHookIfNeeded(targetPackageClassLoader, targetApplicationInfo);
    }

    private static Map<String, XposedInterface.HookHandle> indexOldHookHandles(
            List<XposedInterface.HookHandle> oldHooks) {
        HashMap<String, XposedInterface.HookHandle> out = new HashMap<>();
        for (XposedInterface.HookHandle hookHandle : oldHooks) {
            try {
                String id = hookHandle.getId();
                if (id != null) {
                    out.put(id, hookHandle);
                }
            } catch (Throwable ignored) {
            }
        }
        return out;
    }

    private void unregisterManagedReceiver(Application application, BroadcastReceiver receiver, String tag) {
        if (application == null || receiver == null) {
            return;
        }
        try {
            application.unregisterReceiver(receiver);
        } catch (Throwable th) {
            Log.w("FuseHide", "unregister receiver " + tag, th);
        }
    }

    private void unregisterManagedReceivers(Application application) {
        unregisterManagedReceiver(application, statusReceiver, "status");
        unregisterManagedReceiver(application, configReceiver, "config");
        unregisterManagedReceiver(application, queryReceiver, "query");
        unregisterManagedReceiver(application, systemStateReceiver, "system");
        statusReceiver = null;
        configReceiver = null;
        queryReceiver = null;
        systemStateReceiver = null;
    }

    private void prepareForHotReloadShutdown(String source) {
        CONFIG_RELOAD_EPOCH.incrementAndGet();
        Handler handler = mainHandler;
        if (handler != null) {
            handler.removeCallbacks(configRetryRunnable);
        }
        unregisterManagedReceivers(hookedApplication);
        configLoadInFlight = false;
        Log.d("FuseHide", "prepared hot reload shutdown source=" + source);
    }

    private Object buildSavedInstanceState() {
        HashMap<String, Object> state = new HashMap<>();
        state.put(STATE_HOOKED_APPLICATION, hookedApplication);
        state.put(STATE_TARGET_CLASS_LOADER, targetPackageClassLoader);
        state.put(STATE_TARGET_APPLICATION_INFO, targetApplicationInfo);
        state.put(STATE_TARGET_PACKAGE_NAME, targetPackageName);
        state.put(STATE_CONFIG_COMPLETED, Boolean.valueOf(configLoadCompleted));
        state.put(STATE_PENDING_RETRY_COUNT, Integer.valueOf(pendingConfigRetryCount));
        state.put(STATE_SHOULD_INSTALL_DEBUG_HOOK, Boolean.valueOf(shouldInstallDebugHook));
        return state;
    }

    private void restoreSavedInstanceState(Object savedState) {
        if (!(savedState instanceof Map<?, ?>)) {
            return;
        }
        Map<?, ?> state = (Map<?, ?>) savedState;
        Object savedApplication = state.get(STATE_HOOKED_APPLICATION);
        if (savedApplication instanceof Application) {
            hookedApplication = (Application) savedApplication;
        }
        Object savedClassLoader = state.get(STATE_TARGET_CLASS_LOADER);
        if (savedClassLoader instanceof ClassLoader) {
            targetPackageClassLoader = (ClassLoader) savedClassLoader;
        }
        Object savedApplicationInfo = state.get(STATE_TARGET_APPLICATION_INFO);
        if (savedApplicationInfo instanceof ApplicationInfo) {
            targetApplicationInfo = (ApplicationInfo) savedApplicationInfo;
        }
        Object savedPackageName = state.get(STATE_TARGET_PACKAGE_NAME);
        if (savedPackageName instanceof String) {
            targetPackageName = (String) savedPackageName;
        }
        Object savedConfigCompleted = state.get(STATE_CONFIG_COMPLETED);
        configLoadCompleted = savedConfigCompleted instanceof Boolean && (Boolean) savedConfigCompleted;
        Object savedPendingRetryCount = state.get(STATE_PENDING_RETRY_COUNT);
        pendingConfigRetryCount = savedPendingRetryCount instanceof Integer ? (Integer) savedPendingRetryCount : 0;
        Object savedInstallDebug = state.get(STATE_SHOULD_INSTALL_DEBUG_HOOK);
        shouldInstallDebugHook = savedInstallDebug instanceof Boolean && (Boolean) savedInstallDebug;
    }

    @Override
    public void onModuleLoaded(@androidx.annotation.NonNull ModuleLoadedParam param) {
        Log.d(
                "FuseHide",
                "module loaded process=" + param.getProcessName() + " runtimeHotReload=" + supportsRuntimeHotReload());
    }

    @Override
    public void onPackageLoaded(@androidx.annotation.NonNull PackageLoadedParam param) {
        final String packageName = param.getPackageName();
        if (!PACKAGE_MEDIA.equals(packageName) && !PACKAGE_MEDIA_GOOGLE.equals(packageName)) {
            return;
        }

        System.loadLibrary("fusehide");
        Log.d("FuseHide", "injected");

        final ClassLoader classLoader = param.getDefaultClassLoader();
        targetPackageName = packageName;
        targetPackageClassLoader = classLoader;
        targetApplicationInfo = param.getApplicationInfo();

        advanceNativeGeneration("package_loaded", true);

        hookApplicationAttach();
        installDebugHookIfNeeded(classLoader, param.getApplicationInfo());
    }

    private void hookApplicationAttach() {
        try {
            Method attachMethod = Application.class.getDeclaredMethod("attach", Context.class);
            applicationAttachHookHandle = hookBuilderWithId(attachMethod, HOOK_ID_APPLICATION_ATTACH)
                    .intercept(createApplicationAttachHooker());
        } catch (Throwable th) {
            Log.e("FuseHide", "hook Application.attach", th);
        }
    }

    @Override
    public boolean onHotReloading(@androidx.annotation.NonNull HotReloadingParam param) {
        Log.d(
                "FuseHide",
                "hot reloading frameworkProperties=0x"
                        + Long.toHexString(getFrameworkProperties())
                        + " runtimeHotReload="
                        + supportsRuntimeHotReload());
        param.setSavedInstanceState(buildSavedInstanceState());
        prepareForHotReloadShutdown("hot_reloading");
        return true;
    }

    @Override
    public void onHotReloaded(@androidx.annotation.NonNull HotReloadedParam param) {
        restoreSavedInstanceState(param.getSavedInstanceState());
        Log.d("FuseHide", "hot reloaded oldHooks=" + param.getOldHookHandles().size() + " savedStateRestored=true");

        Map<String, XposedInterface.HookHandle> oldHandles = indexOldHookHandles(param.getOldHookHandles());
        XposedInterface.HookHandle oldAttachHook = oldHandles.remove(HOOK_ID_APPLICATION_ATTACH);
        XposedInterface.HookHandle oldDebugHook = oldHandles.remove(HOOK_ID_DATA_OBB_DEBUG);

        try {
            applicationAttachHookHandle = replaceOrInstallApplicationAttachHook(oldAttachHook);
        } catch (Throwable th) {
            Log.e("FuseHide", "replace/install Application.attach hook", th);
        }

        try {
            dataOrObbDebugHookHandle = replaceOrInstallDebugHook(oldDebugHook);
        } catch (Throwable th) {
            Log.e("FuseHide", "replace/install debug hook", th);
        }

        for (XposedInterface.HookHandle oldHandle : oldHandles.values()) {
            try {
                oldHandle.unhook();
            } catch (Throwable th) {
                Log.w("FuseHide", "unhook stale old handle", th);
            }
        }

        advanceNativeGeneration("hot_reloaded", true);
        configLoadCompleted = false;
        configLoadInFlight = false;
        pendingConfigRetryCount = 0;
        getMainHandler().removeCallbacks(configRetryRunnable);
        Log.d("FuseHide", "hot reloaded scheduling config resync");
        startConfigReload("hot_reloaded");

        if (hookedApplication != null) {
            getMainHandler().post(new MainThreadTask(0, this));
        }
    }

    public void registerStatusReceiver() {
        try {
            Application application = hookedApplication;
            if (application == null) {
                // Fallback: try to get via ActivityThread
                try {
                    Class<?> atClass = Class.forName("android.app.ActivityThread");
                    Method currentAT = atClass.getDeclaredMethod("currentActivityThread");
                    Object at = currentAT.invoke(null);
                    Method getApp = atClass.getDeclaredMethod("getApplication");
                    application = (Application) getApp.invoke(at);
                } catch (Throwable th) {
                    Log.e("FuseHide", "app is null??", th);
                    return;
                }
            }

            hookedApplication = application;
            final Application app = application;
            unregisterManagedReceivers(app);

            StatusBroadcastReceiver receiver = new StatusBroadcastReceiver(app, 0);
            statusReceiver = receiver;
            IntentFilter filter = new IntentFilter(ACTION_GET_STATUS);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                ContextCompat.registerReceiver(app, receiver, filter, ContextCompat.RECEIVER_EXPORTED);
            }

            BroadcastReceiver reloadReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    final PendingResult pendingResult = goAsync();
                    final String requestedToken = intent.getStringExtra(HideConfigStore.EXTRA_RELOAD_TOKEN);
                    final android.os.Bundle bundle = HideConfigStore.loadViaProviderBundle(app);
                    final String providerToken = HideConfigStore.reloadTokenFromBundle(bundle);
                    final boolean providerTokenMatches = requestedToken != null && requestedToken.equals(providerToken);
                    if (bundle != null && providerTokenMatches) {
                        finishConfigReload(app, requestedToken, bundle, "provider", pendingResult);
                        return;
                    }
                    HideConfigStore.requestInjectedProcessConfigBundle(
                            app,
                            fallbackBundle -> finishConfigReload(
                                    app, requestedToken, fallbackBundle, "broadcast_fallback", pendingResult));
                }
            };
            configReceiver = reloadReceiver;
            IntentFilter configFilter = new IntentFilter(HideConfigStore.ACTION_RELOAD_HIDE_CONFIG);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(reloadReceiver, configFilter, Context.RECEIVER_EXPORTED);
            } else {
                ContextCompat.registerReceiver(app, reloadReceiver, configFilter, ContextCompat.RECEIVER_EXPORTED);
            }

            BroadcastReceiver appliedQueryReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    final String queryToken = intent.getStringExtra(HideConfigStore.EXTRA_QUERY_TOKEN);
                    final HideConfig config = currentNativeHideConfig();
                    Intent reply = new Intent(HideConfigStore.ACTION_SET_APPLIED_HIDE_CONFIG)
                            .setPackage(APP_PACKAGE)
                            .putExtra(HideConfigStore.EXTRA_QUERY_TOKEN, queryToken)
                            .putExtras(HideConfigStore.toBundle(config));
                    app.sendBroadcast(reply);
                    Log.d("FuseHide", "reported applied hide config queryToken=" + queryToken);
                }
            };
            queryReceiver = appliedQueryReceiver;
            IntentFilter queryFilter = new IntentFilter(HideConfigStore.ACTION_GET_APPLIED_HIDE_CONFIG);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(appliedQueryReceiver, queryFilter, Context.RECEIVER_EXPORTED);
            } else {
                ContextCompat.registerReceiver(app, appliedQueryReceiver, queryFilter, ContextCompat.RECEIVER_EXPORTED);
            }

            BroadcastReceiver bootStateReceiver = new BroadcastReceiver() {
                @Override
                public void onReceive(Context context, Intent intent) {
                    final String action = intent != null ? intent.getAction() : null;
                    if (action == null || configLoadCompleted) {
                        return;
                    }
                    pendingConfigRetryCount = 0;
                    getMainHandler().removeCallbacks(configRetryRunnable);
                    Log.d("FuseHide", "system config trigger action=" + action);
                    startConfigReload(action);
                }
            };
            systemStateReceiver = bootStateReceiver;
            IntentFilter systemFilter = new IntentFilter();
            systemFilter.addAction(Intent.ACTION_LOCKED_BOOT_COMPLETED);
            systemFilter.addAction(Intent.ACTION_BOOT_COMPLETED);
            systemFilter.addAction(Intent.ACTION_USER_UNLOCKED);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(bootStateReceiver, systemFilter, Context.RECEIVER_NOT_EXPORTED);
            } else {
                ContextCompat.registerReceiver(
                        app, bootStateReceiver, systemFilter, ContextCompat.RECEIVER_NOT_EXPORTED);
            }

            startConfigReload("initial");
            Log.d("FuseHide", "registered");
        } catch (Throwable th) {
            Log.e("FuseHide", "register", th);
        }
    }
}
