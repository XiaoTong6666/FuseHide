package fusefixer;

import io.github.xiaotong6666.fusefixer.Entry;
import io.github.xiaotong6666.fusefixer.MainActivity;

public final class MainThreadTask implements Runnable {
    private final int taskKind;
    private final Object target;

    public MainThreadTask(int taskKind, Object target) {
        this.taskKind = taskKind;
        this.target = target;
    }

    @Override
    public void run() {
        if (taskKind == 0) {
            ((Entry) target).registerStatusReceiver();
        } else {
            ((MainActivity) target).onHookCheckTimeout();
        }
    }
}
