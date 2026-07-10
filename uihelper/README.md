# uihelper

`uihelper` is an Android UI framework for settings-oriented and utility-style apps.

Its design goal is:

- write business pages once
- switch between `Material` and `Miuix` skins behind one abstraction layer
- keep project-specific widgets and business models out of the framework

This module is intended to be extractable and reusable in other projects.

## Scope

`uihelper` should own:

- UI mode and skin runtime
- app chrome and shared shell behavior
- reusable adaptive UI components
- skin-specific primitive implementations
- optional Android-app extensions that are still generic enough to reuse

`uihelper` should not own:

- app-specific data models
- business page semantics
- feature-specific widgets
- project-specific actions, routes, or search state machines

Examples of code that does **not** belong in `uihelper`:

- `AppInfo`
- `GroupedApps`
- `SearchStatus` tied to one feature flow
- `ConfigPageOverflowAction` tied to one app's config page
- app list rows, config detail cards, or any widget named after one project's domain nouns

## Package Contract

### Public API

These packages are the supported surface for feature and page code.

- `io.github.xiaotong6666.uihelper.adaptive`
  - semantic cross-skin components
  - preferred import target for business pages
- `io.github.xiaotong6666.uihelper.common`
  - small reusable UI helpers shared across skins
- `io.github.xiaotong6666.uihelper.model`
  - generic UI models and enums used by adaptive components
- `io.github.xiaotong6666.uihelper.mode`
  - UI mode runtime such as `UiMode` and `LocalUiMode`
- `io.github.xiaotong6666.uihelper.chrome`
  - shell and nested-scroll integration used by app-level containers
  - includes the public dual-skin host API for top-level navigation shells
  - public entry points should be shell-style APIs such as `AdaptiveNavigationShell`, `PageHost`, and `PageChrome`
- `io.github.xiaotong6666.uihelper.navigation3`
  - reusable navigator helpers
- `io.github.xiaotong6666.uihelper.extensions.androidapp`
  - optional Android-app-specific helpers that remain generic enough to reuse

### Implementation Layers

These packages are framework internals and should normally only be used by `uihelper` itself or by app-specific skin widgets.

- `io.github.xiaotong6666.uihelper.material.scaffold`
- `io.github.xiaotong6666.uihelper.material.primitive`
- `io.github.xiaotong6666.uihelper.miuix.primitive`

Within `chrome`, shell internals such as composition locals and host state objects should stay internal to the module even though the package itself is public.

Rules:

- business pages should not import these packages directly
- if a feature page needs something from here more than once, add or extend an adaptive/common API instead
- app-specific Material or Miuix widgets may depend on these packages when they are intentionally skin-specific

## Dependency Direction

The intended dependency flow is:

`feature page -> adaptive/common/model -> material primitive or miuix primitive`

Not this:

`feature page -> material primitive`

And never this:

`uihelper -> app project feature code`

## Placement Rules

When deciding whether code belongs in `uihelper`, use this test:

> If the project name and business data changed, would this code still make sense?

If yes, it can belong in `uihelper`.

If no, keep it in the app project.

More specific guidance:

- put semantic cross-skin APIs in `adaptive`
- put tiny reusable visual helpers in `common`
- put generic presentation models in `model`
- put Material-only building blocks in `material.primitive`
- put Miuix-only building blocks in `miuix.primitive`
- put app-shell behavior in `chrome`
- put Android `PackageManager` or app-icon helpers in `extensions.androidapp` only if they stay project-agnostic

## Expected Usage

Business page code should look like this:

```kotlin
import io.github.xiaotong6666.uihelper.adaptive.SectionCard
import io.github.xiaotong6666.uihelper.adaptive.SectionTitle
import io.github.xiaotong6666.uihelper.adaptive.SettingsToggleItem
import io.github.xiaotong6666.uihelper.model.SectionTitleStyle
```

Not like this:

```kotlin
import io.github.xiaotong6666.uihelper.material.primitive.SettingsToggleItemMaterial
import io.github.xiaotong6666.uihelper.miuix.primitive.SettingsToggleItemMiuix
```

App shell code may use runtime and chrome packages directly:

```kotlin
import io.github.xiaotong6666.uihelper.chrome.AdaptiveNavigationShell
import io.github.xiaotong6666.uihelper.chrome.PageChrome
import io.github.xiaotong6666.uihelper.mode.UiMode
```

## Maintenance Rules

Before adding new code to `uihelper`:

1. Decide whether it is public API, implementation layer, or app-specific code.
2. Prefer extending `adaptive` over exposing a new skin-specific primitive to business pages.
3. Reject project-specific nouns in framework packages.
4. Keep `material/primitive` and `miuix/primitive` behavior aligned under one semantic contract.

If a future change makes a business page import `material.primitive` or `miuix.primitive` directly, treat that as architectural debt and either:

- lift the abstraction into `adaptive`, or
- keep the logic inside an app-specific skin widget layer
