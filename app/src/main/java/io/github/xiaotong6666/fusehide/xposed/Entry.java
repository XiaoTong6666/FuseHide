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
import io.github.libxposed.api.XposedModule;
import io.github.libxposed.api.XposedModuleInterface.PackageLoadedParam;
import io.github.xiaotong6666.fusehide.config.HideConfig;
import io.github.xiaotong6666.fusehide.config.HideConfigNativeBridge;
import io.github.xiaotong6666.fusehide.config.HideConfigStore;
import io.github.xiaotong6666.fusehide.config.PackageHideRule;
import io.github.xiaotong6666.fusehide.status.StatusBroadcastReceiver;
import java.lang.reflect.Method;

public class Entry extends XposedModule {
    private static final String APP_PACKAGE = "io.github.xiaotong6666.fusehide";
    private static final String ACTION_GET_STATUS = APP_PACKAGE + ".GET_STATUS";
    private static final String PACKAGE_MEDIA = "com.android.providers.media.module";
    private static final String PACKAGE_MEDIA_GOOGLE = "com.google.android.providers.media.module";
    private static final long CONFIG_RETRY_DELAY_MS = 15000L;
    private static final int CONFIG_MAX_RETRIES = 8;

    private volatile Handler mainHandler;
    private volatile Application hookedApplication;
    private boolean configLoadCompleted;
    private boolean configLoadInFlight;
    private int pendingConfigRetryCount;
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
        HideConfigStore.reloadInjectedProcessConfig(application, applied -> onConfigReloadFinished(source, applied));
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

        hookApplicationAttach(classLoader);

        if ((param.getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
            try {
                Class<?> mediaProviderClass = classLoader.loadClass("com.android.providers.media.MediaProvider");
                Method targetMethod = mediaProviderClass.getMethod(
                        "isUidAllowedAccessToDataOrObbPathForFuse", int.class, String.class);
                hook(targetMethod).intercept(chain -> {
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
                });
            } catch (Throwable th) {
                Log.e("FuseHide", "hook isUidAllowedAccessToDataOrObbPathForFuse", th);
            }
        }
    }

    private void hookApplicationAttach(ClassLoader classLoader) {
        try {
            Class<?> applicationClass = classLoader.loadClass("android.app.Application");
            Method attachMethod = applicationClass.getDeclaredMethod("attach", Context.class);
            hook(attachMethod).intercept(chain -> {
                Object result = chain.proceed();
                if (hookedApplication == null) {
                    Application app = (Application) chain.getThisObject();
                    hookedApplication = app;
                    Log.d("FuseHide", "captured Application via attach hook");
                    new Handler(Looper.getMainLooper()).post(new MainThreadTask(0, this));
                }
                return result;
            });
        } catch (Throwable th) {
            Log.e("FuseHide", "hook Application.attach", th);
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
            StatusBroadcastReceiver receiver = new StatusBroadcastReceiver(app, 0);
            IntentFilter filter = new IntentFilter(ACTION_GET_STATUS);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                ContextCompat.registerReceiver(app, receiver, filter, ContextCompat.RECEIVER_EXPORTED);
            }

            BroadcastReceiver configReceiver = new BroadcastReceiver() {
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
            IntentFilter configFilter = new IntentFilter(HideConfigStore.ACTION_RELOAD_HIDE_CONFIG);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(configReceiver, configFilter, Context.RECEIVER_EXPORTED);
            } else {
                ContextCompat.registerReceiver(app, configReceiver, configFilter, ContextCompat.RECEIVER_EXPORTED);
            }

            BroadcastReceiver queryReceiver = new BroadcastReceiver() {
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
            IntentFilter queryFilter = new IntentFilter(HideConfigStore.ACTION_GET_APPLIED_HIDE_CONFIG);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(queryReceiver, queryFilter, Context.RECEIVER_EXPORTED);
            } else {
                ContextCompat.registerReceiver(app, queryReceiver, queryFilter, ContextCompat.RECEIVER_EXPORTED);
            }

            BroadcastReceiver systemStateReceiver = new BroadcastReceiver() {
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
            IntentFilter systemFilter = new IntentFilter();
            systemFilter.addAction(Intent.ACTION_LOCKED_BOOT_COMPLETED);
            systemFilter.addAction(Intent.ACTION_BOOT_COMPLETED);
            systemFilter.addAction(Intent.ACTION_USER_UNLOCKED);
            if (Build.VERSION.SDK_INT >= 33) {
                app.registerReceiver(systemStateReceiver, systemFilter, Context.RECEIVER_NOT_EXPORTED);
            } else {
                ContextCompat.registerReceiver(
                        app, systemStateReceiver, systemFilter, ContextCompat.RECEIVER_NOT_EXPORTED);
            }

            startConfigReload("initial");
            Log.d("FuseHide", "registered");
        } catch (Throwable th) {
            Log.e("FuseHide", "register", th);
        }
    }
}
