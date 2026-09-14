```mermaid
%%{init: {'flowchart': { 'curve': 'linear', 'nodeSpacing': 26, 'rankSpacing': 42 }}}%%
flowchart TD
    classDef default fill:none,stroke:#555,stroke-width:2px,color:#ffffff;
    classDef inject fill:none,stroke:#01579b,stroke-width:2px,color:#ffffff;
    classDef config fill:none,stroke:#33691e,stroke-width:2px,stroke-dasharray:5 5,color:#ffffff;
    classDef policy fill:none,stroke:#b71c1c,stroke-width:2px,color:#ffffff;
    classDef native fill:none,stroke:#4a148c,stroke-width:2px,color:#ffffff;
    classDef elf fill:none,stroke:#e65100,stroke-width:2px,color:#ffffff;
    classDef fuse fill:none,stroke:#1b5e20,stroke-width:2px,color:#ffffff;
    classDef cache fill:none,stroke:#ad1457,stroke-width:2px,stroke-dasharray:4 4,color:#ffffff;
    classDef os fill:none,stroke:#546e7a,stroke-width:2px,color:#ffffff;

    subgraph Phase1 ["第一层：双注入后端、仲裁与 MediaProvider 生命周期"]
        direction TB

        PostFs["Zygisk module post-fs-data.sh"] --> LspDb{"LSPosed 中 FuseHide 是否启用<br/>且作用域包含 MediaProvider"}
        LspDb -->|是| LspFlag["创建 disable + lsp_scope_enabled<br/>禁止 Zygisk 与 LSPosed 重复注入"]
        LspDb -->|否| ZygiskAvailable["允许 Zygisk backend"]

        LSP["LSPosed / libxposed"] -->|onPackageLoaded| Entry["Entry.onPackageLoaded"]
        Entry --> TargetLsp{"包名是否为 AOSP / Google MediaProvider"}
        TargetLsp -->|否| LspIgnore["忽略"]
        TargetLsp -->|是| LoadSo["System.loadLibrary(libfusehide.so)"]
        LoadSo --> LspJniOnLoad["JNI_OnLoad<br/>保存 MediaProvider JavaVM"]
        Entry --> AttachHook["Hook Application.attach"]
        AttachHook --> MediaApp["捕获 MediaProvider Application"]
        MediaApp --> LspJava["MainThreadTask<br/>Entry.registerStatusReceiver"]

        Zygisk["Zygisk ModuleBase"] --> ZOnLoad["onLoad<br/>保存 Api + JavaVM"]
        ZOnLoad --> ZPre["preAppSpecialize"]
        ZPre --> TargetZ{"nice_name 是否为 MediaProvider"}
        TargetZ -->|否| ZClose["DLCLOSE_MODULE_LIBRARY"]
        TargetZ -->|是| ScopeGate{"lsp_scope_enabled 是否存在"}
        LspFlag -.-> ScopeGate
        ZygiskAvailable -.-> ScopeGate
        ScopeGate -->|是| ZSkip["跳过 Zygisk backend"]
        ScopeGate -->|否| Preload["PreloadModuleRuntime<br/>PreloadInjectedDex + android_dlopen_ext(libfusehide.so)"]
        Preload --> ZPost["postAppSpecialize"]
        ZPost --> DlopenMonitor["InstallDlopenMonitor<br/>DobbyHook linker do_dlopen"]
        DlopenMonitor --> FuseLoaded["观察 libfuse_jni.so 加载"]
        FuseLoaded --> ZBootstrap["JNI_OnLoad + native_init(DobbyHookAdapter)<br/>RegisterAllNativeMethods"]
        ZBootstrap --> StartJava["StartInjectedJavaWhenApplicationReady"]
        StartJava --> ZEntry["ZygiskEntry.init(Context)"]
        ZEntry --> ZJava["ZygiskEntry.registerStatusReceiver"]

        LSP -->|native_init.list: libfusehide.so| LspNativeApi["libxposed native_init(api)<br/>保存 api.hookFunc"]
        LspNativeApi -->|libxposed 通知 loadedLibrary| SharedPostInit["PostNativeInit(loadedLibrary)"]
        ZBootstrap --> SharedPostInit
        SharedPostInit -->|libfuse_jni.so| InstallHooks["InstallFuseHooks"]

        LspJava --> JavaRuntime["共享 Java 控制面"]
        ZJava --> JavaRuntime
    end
    class PostFs,LspDb,LspFlag,ZygiskAvailable,LSP,Entry,TargetLsp,LspIgnore,LoadSo,LspJniOnLoad,AttachHook,MediaApp,LspJava,Zygisk,ZOnLoad,ZPre,TargetZ,ZClose,ScopeGate,ZSkip,Preload,ZPost,DlopenMonitor,FuseLoaded,ZBootstrap,StartJava,ZEntry,ZJava,LspNativeApi,SharedPostInit,JavaRuntime inject;
    class InstallHooks native;

    subgraph Phase2 ["第二层：共享 Java 控制面与经过认证的配置同步"]
        direction TB

        UI["FuseHide App UI"] --> Store["HideConfigStore"]
        Store --> AppPrefs[("Device-protected SharedPreferences<br/>saved config + reload token")]
        Store --> Provider["HideConfigProvider<br/>get_hide_config"]
        Store --> RequestReceiver["HideConfigRequestReceiver"]
        Store --> ReloadBc["ACTION_RELOAD_HIDE_CONFIG<br/>携带 reloadToken"]
        Store --> QueryBc["ACTION_GET_APPLIED_HIDE_CONFIG<br/>携带 queryToken"]

        JavaRuntime --> ReceiverHub["注册 injected-process Receiver 集合"]
        ReceiverHub --> StatusRx["StatusBroadcastReceiver"]
        ReceiverHub --> ReloadRx["配置 reload Receiver"]
        ReceiverHub --> QueryRx["已应用配置 query Receiver"]
        ReceiverHub --> BootRx["LOCKED_BOOT / BOOT / USER_UNLOCKED<br/>触发重试"]
        ReceiverHub --> PackageRx["PACKAGE_ADDED / PACKAGE_REMOVED<br/>忽略 replacement churn"]
        ReceiverHub --> InitialReload["initial reload"]

        InitialReload --> ReloadFlow["reloadInjectedProcessConfig"]
        BootRx --> ReloadFlow
        ReloadFlow --> Snapshot["先加载 injected-process snapshot"]
        Snapshot --> SnapshotApply["applyBundleToNative(snapshot)<br/>允许早期恢复策略"]
        SnapshotApply --> ProviderTry["读取 HideConfigProvider"]
        ProviderTry --> ProviderMatch{"provider token 与 snapshot<br/>是否已经一致"}
        ProviderMatch -->|是| ReloadDone["无需重复 JNI apply"]
        ProviderMatch -->|否| ProviderApply["applyBundleToNative(provider)"]
        ProviderTry -->|失败| AuthFallback["requestInjectedProcessConfigBundle<br/>随机 action + queryToken + one-shot mutable PendingIntent"]
        AuthFallback --> RequestReceiver
        RequestReceiver --> AuthGate{"校验 PendingIntent<br/>broadcast / mutable / user / creatorPackage / creatorUid"}
        AuthGate -->|可信 MediaProvider| PiReply["PendingIntent.send(config + token)"]
        AuthGate -->|失败| Reject["拒绝请求"]
        PiReply --> FallbackApply["applyBundleToNative(fallback)"]

        ReloadBc -.-> ReloadRx
        ReloadRx --> ExplicitProvider["provider 优先"]
        ExplicitProvider --> ExplicitToken{"bundleToken == requestedToken"}
        ExplicitToken -->|是| ExplicitApply["applyBundleToNative"]
        ExplicitToken -->|否 / provider 失败| AuthFallback

        ProviderApply --> SaveSnapshot["saveInjectedProcessSnapshot"]
        FallbackApply --> SaveSnapshot
        ExplicitApply --> SaveSnapshot
        SaveSnapshot --> Ack["ACTION_SET_CONFIG_STATUS<br/>返回 applied / message / token"]
        Ack -.-> UI

        QueryBc -.-> QueryRx
        QueryRx --> NativeSnapshot["从 HideConfigNativeBridge<br/>读取 CurrentHideConfig"]
        NativeSnapshot --> QueryReply["ACTION_SET_APPLIED_HIDE_CONFIG"]
        QueryReply -.-> UI

        PackageRx --> PackageChanged["HideConfigNativeBridge.notifyPackageSetChanged"]
    end
    class UI,Store,AppPrefs,Provider,RequestReceiver,ReloadBc,QueryBc,ReceiverHub,StatusRx,ReloadRx,QueryRx,BootRx,PackageRx,InitialReload,ReloadFlow,Snapshot,SnapshotApply,ProviderTry,ProviderMatch,ReloadDone,ProviderApply,AuthFallback,AuthGate,PiReply,Reject,FallbackApply,ExplicitProvider,ExplicitToken,ExplicitApply,SaveSnapshot,Ack,NativeSnapshot,QueryReply,PackageChanged config;

    subgraph Phase3 ["第三层：Native 配置编译、UID→Package 解析与有效规则"]
        direction TB

        SnapshotApply --> JNIApply["HideConfigNativeBridge.applyHideConfig"]
        ProviderApply --> JNIApply
        FallbackApply --> JNIApply
        ExplicitApply --> JNIApply
        JNIApply --> ApplyConfig["ApplyHideConfig"]
        ApplyConfig --> PublishConfig["原子发布<br/>CurrentHideConfig + CompiledHideConfig"]
        ApplyConfig --> ConfigGen["gHideConfigGeneration++"]
        ApplyConfig --> Compile["BuildCompiledHideConfig"]
        Compile --> GlobalFragment["globalRuleFragment<br/>global targets + hide-all exemptions"]
        Compile --> PackageFragments["packageRuleFragments<br/>[package.name] 专属规则"]
        Compile --> HiddenPkgSet["hiddenPackageSet<br/>决定哪些包继承 global rule"]

        PackageChanged --> PackageGen["NotifyUidRulePackageSetChanged<br/>gUidPackageSetGeneration++"]

        ReqUidSource["FUSE request uid"] --> ResolveUid["ResolveHideRuleForUid(uid)"]
        ResolveUid --> UidCache{"gUidHideRuleCache<br/>configGeneration + packageSetGeneration 命中?"}
        UidCache -->|命中| EffectiveRule["CompiledHideRule<br/>UID 的有效隐藏策略 + fingerprint"]
        UidCache -->|未命中| PM["PackageManager.getPackagesForUid(uid)"]
        PM --> Merge["排序/去重 package set<br/>hiddenPackages 命中→合并 global<br/>逐包合并 package-specific fragments"]
        GlobalFragment -.-> Merge
        PackageFragments -.-> Merge
        HiddenPkgSet -.-> Merge
        Merge --> BuildRule["BuildCompiledHideRule<br/>canonical sets + relative prefixes + fingerprint"]
        BuildRule --> UidCacheStore["写入 UID rule cache"]
        UidCacheStore --> EffectiveRule
    end
    class JNIApply,ApplyConfig,PublishConfig,ConfigGen,Compile,GlobalFragment,PackageFragments,HiddenPkgSet,PackageGen,ReqUidSource,ResolveUid,UidCache,EffectiveRule,PM,Merge,BuildRule,UidCacheStore policy;

    subgraph Phase4 ["第四层：Native Hook 初始化、ABI Gate 与解析策略"]
        direction TB

        InstallHooks --> ModuleMap["FindTargetModule(libfuse_jni.so)<br/>/proc/self/maps"]
        ModuleMap --> HookPlan["ResolveDeviceHookInstallPlan"]

        HookPlan --> ReqCtxResolve["解析 fuse_req_ctx<br/>RTLD_DEFAULT → bound relocation → libfuse.so symbol"]
        ReqCtxResolve --> ReqCtxGate{"解析成功?"}
        ReqCtxGate -->|是| ReqCtxFn["Process.fuseReqCtx<br/>RuntimeState::ReqUid 使用官方函数"]
        ReqCtxGate -->|否| ReqCtxFail["fail closed<br/>不再读取未经验证的 fuse_req 内部偏移"]

        HookPlan --> StringGate{"libc++ string ABI<br/>是否通过可信 profile 验证"}
        StringGate -->|否| SkipString["跳过需要解码/构造<br/>std::string object 的 Hook"]
        StringGate -->|是| AllowString["允许 string ABI Hook"]

        HookPlan --> DirAbi["DetectDirectoryEntriesAbi"]
        DirAbi --> DirAbiGate{"shared_ptr / value / unknown"}
        DirAbiGate -->|shared_ptr| SharedDir["shared_ptr DirectoryEntries wrappers"]
        DirAbiGate -->|value| ValueDir["API 37 value-vector wrappers<br/>缓存仅保存 name/type 语义快照"]
        DirAbiGate -->|unknown| SkipDir["跳过不安全的 C++ container Hook"]

        ModuleMap --> Embedded{"模块路径是否为 embedded APEX (!/)"}
        Embedded -->|否| FileElf["BuildFileElfContext<br/>先装 minimal file-backed hooks"]
        FileElf --> Minimal["InstallMinimalCoreHooks<br/>InstallMinimalDebugHooks"]
        Embedded -->|是| Advanced["直接进入 advanced resolver"]
        Minimal --> CoreComplete{"core hooks 是否完整"}
        CoreComplete -->|否| Advanced
        CoreComplete -->|是| DebugHooks["InstallAdvancedDebugHooks"]
        Advanced --> Sources["runtime ELF / file ELF / relocations<br/>.gnu_debugdata / resolved anchors / trusted profile fallback"]
        Sources --> AdvancedInstall["InstallAdvancedCoreHooks"]
        AdvancedInstall --> DebugHooks

        LspNativeApi -.-> InstallerApi["gHookInstaller"]
        ZBootstrap -.-> InstallerApi
        InstallerApi --> HookBackend{"实际 inline hook backend"}
        HookBackend -->|LSPosed| LspHook["libxposed api.hookFunc"]
        HookBackend -->|Zygisk| DobbyHookNode["DobbyHookAdapter"]
        DebugHooks --> HookSet["最终 Hook 集合"]
    end
    class ModuleMap,HookPlan,ReqCtxResolve,ReqCtxGate,ReqCtxFn,ReqCtxFail,StringGate,SkipString,AllowString,DirAbi,DirAbiGate,SharedDir,ValueDir,SkipDir,InstallerApi,HookBackend,LspHook,DobbyHookNode,HookSet native;
    class Embedded,FileElf,Minimal,CoreComplete,Advanced,Sources,AdvancedInstall,DebugHooks elf;

    subgraph Phase5 ["第五层：Hook 覆盖面"]
        direction TB

        HookSet --> PolicyHooks["路径 / 特殊存储 / 缓存策略<br/>is_app_accessible_path<br/>is_package_owned_path / is_bpf_backing_path<br/>ShouldNotCache / strcasecmp / EqualsIgnoreCase"]
        HookSet --> RequestHooks["FUSE handler<br/>pf_lookup / lookup_postfilter / access<br/>open / opendir / getattr<br/>readdir / readdirplus / readdir_postfilter<br/>do_readdir_common<br/>mkdir / mknod / create / unlink / rmdir / rename"]
        HookSet --> DirHooks["目录容器<br/>GetDirectoryEntries<br/>addDirectoryEntriesFromLowerFs"]
        HookSet --> ReplyHooks["FUSE reply<br/>fuse_reply_entry / attr / buf / err<br/>notify_inval_entry / notify_inval_inode"]
        HookSet --> LibcHooks["lower-fs / libc fallback<br/>stat / lstat / getxattr / lgetxattr<br/>open / __open_2 / mkdir / mknod"]
        PolicyHooks --> AndroidSpecial["Android/data / Android/obb 特殊链<br/>UnicodePolicy 去除 Default_Ignorable<br/>保持 MediaProvider 专用访问控制语义"]
    end
    class PolicyHooks,RequestHooks,DirHooks,ReplyHooks,LibcHooks native;
    class AndroidSpecial policy;

    subgraph Phase6 ["第六层：FUSE 数据面——请求判定、目录过滤与错误语义"]
        direction TB

        Caller(("受限 App")) -->|syscall 访问 /sdcard| VFS["Kernel VFS"]
        VFS --> DevFuse["/dev/fuse"]
        DevFuse --> Worker["MediaProvider libfuse_jni worker"]
        Worker --> Session["ScopedFuseRequestSession<br/>RememberFuseSession"]
        Session --> ReqUid["RuntimeState::ReqUid(req)<br/>fuse_req_ctx(req)->uid"]
        ReqUid --> ReqUidSource
        ReqUid --> EffectiveRule
        EffectiveRule --> Gate{"请求类型"}

        Gate -->|lookup/access/open/getattr/mutation| NamedPath["ClassifyHiddenNamedTarget / ClassifyHiddenPath<br/>exact root + descendant policy"]
        NamedPath --> HiddenGate{"命中隐藏策略?"}
        HiddenGate -->|是| HiddenReply["优先 fuse_reply_err<br/>ENOENT / EPERM 等隐藏语义"]
        HiddenGate -->|否| OriginalPath["调用原始 handler / lower-fs"]
        HiddenReply --> ErrBridge["ReplyErrorBridge<br/>无法直接 reply 时 arm errno remap"]
        ErrBridge --> ErrRemap["WrappedReplyErr<br/>修正 EEXIST / EISDIR / ENOTEMPTY / ENOTDIR 等存在性泄漏"]

        Gate -->|readdir 家族| ReaddirCtx["记录 PendingReaddirContext<br/>req.unique + uid + ino + path"]
        ReaddirCtx --> DirRead["WrappedGetDirectoryEntries<br/>按 UID 读取/过滤目录"]
        DirRead --> LowerDir["addDirectoryEntriesFromLowerFs<br/>再次过滤 lower-fs 追加项"]
        LowerDir --> ReplyBuf["WrappedReplyBuf<br/>最终 universal wire filter"]
        ReplyBuf --> RecoverCtx["恢复 uid / ino / parent path<br/>必要时使用 recent hidden parent fallback"]
        RecoverCtx --> WireDetect["识别 dirent / direntplus / fuse_read_out<br/>plain / plus / postfilter / auto fallback"]
        WireDetect --> Rewrite["移除隐藏条目<br/>重写 payload size / buffer"]
        Rewrite --> OriginalReplyBuf["调用原始 fuse_reply_buf"]

        OriginalPath --> PositiveReply["fuse_reply_entry / fuse_reply_attr"]
        PositiveReply --> LearnPath["学习 inode→path / hidden-subtree inode<br/>记录 visible root parent"]
        LearnPath --> Caller
        OriginalReplyBuf --> Caller
        ErrRemap --> Caller
    end
    class Caller,VFS,DevFuse,Worker os;
    class Session,ReqUid,Gate,NamedPath,HiddenGate,HiddenReply,OriginalPath,ErrBridge,ErrRemap,ReaddirCtx,DirRead,LowerDir,ReplyBuf,RecoverCtx,WireDetect,Rewrite,OriginalReplyBuf,PositiveReply,LearnPath fuse;

    subgraph Phase7 ["第七层：缓存、Generation 与跨 UID 一致性维护"]
        direction TB

        CacheHub["运行时一致性层"] --> UidRuleCache["UID rule cache<br/>key=uid<br/>guard=configGeneration + packageSetGeneration"]
        CacheHub --> PathClassCache["HiddenPathClassification cache<br/>key=uid + path<br/>guard=configGeneration + packageSetGeneration"]
        CacheHub --> RootSnapshot["RootSnapshot cache<br/>key=uid + effective-rule fingerprint + root path<br/>guard=config/package/parent generations"]
        RootSnapshot --> CollisionGate["fingerprint 命中后再做<br/>CompiledHideRulesSemanticallyEqual<br/>碰撞只变 cache miss"]
        CacheHub --> InodeCache["inode→path cache<br/>有上限并保护隐藏目标/祖先路径"]
        CacheHub --> HiddenInodes["hidden-subtree inode → rule 集合<br/>避免跨 UID 策略串用"]
        CacheHub --> PendingState["pending readdir context / recent hidden parent<br/>UID-aware fallback state"]

        ConfigGen --> ClearConfigCaches["清 UID rule / root snapshot / path classification<br/>invalidate tracked hidden targets"]
        PackageGen --> ClearPackageCaches["清 UID 派生 cache<br/>root snapshot / path classification<br/>invalidate tracked targets"]
        ClearConfigCaches --> CacheHub
        ClearPackageCaches --> CacheHub

        DirRead --> RootSnapshot
        NamedPath --> PathClassCache
        ResolveUid --> UidRuleCache
        LearnPath --> InodeCache
        LearnPath --> HiddenInodes
        ReaddirCtx --> PendingState

        RequestHooks --> MutationTrack["root mutation request<br/>记录 pending mutation"]
        MutationTrack --> MutationReply["WrappedReplyErr 确认 err==0 后<br/>gRootSnapshotParentGeneration++"]
        MutationReply --> RootSnapshot

        ReplyHooks --> CacheControl["WrappedReplyEntry / Attr / ShouldNotCache<br/>entry_timeout=0 / attr_timeout=0 / no-cache"]
        CacheControl --> Invalidate["ScheduleHiddenEntryInvalidation<br/>ScheduleSpecificEntryInvalidation<br/>ScheduleHiddenInodeInvalidation"]
        HiddenReply --> Invalidate
        Rewrite --> Invalidate
        Invalidate --> KernelCache["fuse_lowlevel_notify_inval_entry / inode<br/>尽快压掉共享 dentry / inode cache"]

        Session --> SessionChange{"FUSE session 是否变化"}
        SessionChange -->|变化| ClearSession["清 session-scoped tracking<br/>inode / subtree / pending / errno-remap 等状态"]
        ClearSession --> CacheHub
    end
    class CacheHub,UidRuleCache,PathClassCache,RootSnapshot,CollisionGate,InodeCache,HiddenInodes,PendingState,ClearConfigCaches,ClearPackageCaches,MutationTrack,MutationReply,CacheControl,Invalidate,KernelCache,SessionChange,ClearSession cache;

    style Phase1 fill:none,stroke:#01579b,stroke-width:2px,stroke-dasharray:5 5
    style Phase2 fill:none,stroke:#33691e,stroke-width:2px,stroke-dasharray:5 5
    style Phase3 fill:none,stroke:#b71c1c,stroke-width:2px,stroke-dasharray:5 5
    style Phase4 fill:none,stroke:#4a148c,stroke-width:2px,stroke-dasharray:5 5
    style Phase5 fill:none,stroke:#e65100,stroke-width:2px,stroke-dasharray:5 5
    style Phase6 fill:none,stroke:#1b5e20,stroke-width:2px,stroke-dasharray:5 5
    style Phase7 fill:none,stroke:#ad1457,stroke-width:2px,stroke-dasharray:5 5
```
