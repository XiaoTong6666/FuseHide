package de.robv.android.xposed;

import de.robv.android.xposed.callbacks.XC_LoadPackage;

public interface IXposedHookLoadPackage extends IXposedMod {
    void handleLoadPackage(XC_LoadPackage.LoadPackageParam loadPackageParam) throws Throwable;
}
