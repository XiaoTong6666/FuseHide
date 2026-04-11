package android.app;

public final class AndroidAppHelper {
    private AndroidAppHelper() {}

    public static Application currentApplication() {
        try {
            Class<?> activityThread = Class.forName("android.app.ActivityThread");
            return (Application) activityThread.getMethod("currentApplication").invoke(null);
        } catch (Throwable ignored) {
            return null;
        }
    }
}
