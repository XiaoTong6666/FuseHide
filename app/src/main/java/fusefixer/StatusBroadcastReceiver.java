package fusefixer;

import android.app.Application;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;
import io.github.xiaotong6666.fusefixer.MainActivity;

public final class StatusBroadcastReceiver extends BroadcastReceiver {
    private static final String APP_PACKAGE = "io.github.xiaotong6666.fusefixer";
    private static final String ACTION_GET_STATUS = APP_PACKAGE + ".GET_STATUS";
    private static final String ACTION_SET_STATUS = APP_PACKAGE + ".SET_STATUS";

    private final int mode;
    private final ContextWrapper owner;

    public StatusBroadcastReceiver(ContextWrapper owner, int mode) {
        this.mode = mode;
        this.owner = owner;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (mode == 0) {
            handleGetStatus(intent);
        } else {
            handleSetStatus(intent);
        }
    }

    private void handleGetStatus(Intent intent) {
        Application application = (Application) owner;
        try {
            Log.d("LSPosedFuseFixer", "recv " + intent);
            PendingIntent pendingIntent = intent.getParcelableExtra("EXTRA_PENDING_INTENT");
            if (pendingIntent == null) {
                Log.e("LSPosedFuseFixer", "no pendingintent?");
                return;
            }
            if (!APP_PACKAGE.equals(pendingIntent.getCreatorPackage())) {
                Log.e("LSPosedFuseFixer", "invalid pkg " + pendingIntent.getCreatorPackage());
                return;
            }

            Intent statusIntent = new Intent(ACTION_SET_STATUS).setPackage(APP_PACKAGE);
            statusIntent.putExtra(
                    "EXTRA_PENDING_INTENT", PendingIntent.getBroadcast(application, 1, statusIntent, 67108864));
            statusIntent.putExtra("EXTRA_PID", Process.myPid());
            Bundle extras = intent.getExtras();
            if (extras != null) {
                Bundle outExtras = statusIntent.getExtras();
                if (outExtras != null) {
                    outExtras.putBinder("EXTRA_BINDER", extras.getBinder("EXTRA_BINDER"));
                }
            }
            application.sendBroadcast(statusIntent);
        } catch (Throwable th) {
            Log.e("FuseFixer", "send: ", th);
        }
    }

    private void handleSetStatus(Intent intent) {
        MainActivity mainActivity = (MainActivity) owner;
        try {
            Log.d("LSPosedFuseFixer", "recv status " + intent);
            PendingIntent pendingIntent = intent.getParcelableExtra("EXTRA_PENDING_INTENT");
            if (pendingIntent == null) {
                Log.e("LSPosedFuseFixer", "status pendingintent missing");
                return;
            }
            String creatorPackage = pendingIntent.getCreatorPackage();
            if (!"com.android.providers.media.module".equals(creatorPackage)
                    && !"com.google.android.providers.media.module".equals(creatorPackage)) {
                Log.e("LSPosedFuseFixer", "status invalid creator " + creatorPackage);
                return;
            }
            Log.d(
                    "LSPosedFuseFixer",
                    "status accepted from " + creatorPackage + " pid=" + intent.getIntExtra("EXTRA_PID", -1));
            mainActivity.onHookStatusReceived(creatorPackage, intent.getIntExtra("EXTRA_PID", -1));
        } catch (Throwable th) {
            Log.e("LSPosedFuseFixer", "send: ", th);
        }
    }
}
