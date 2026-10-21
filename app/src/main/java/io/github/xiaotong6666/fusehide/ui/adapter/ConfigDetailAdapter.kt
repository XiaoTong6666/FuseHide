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

package io.github.xiaotong6666.fusehide.ui.adapter

import io.github.xiaotong6666.fusehide.config.HideConfig
import io.github.xiaotong6666.fusehide.config.HideConfigDefaults
import io.github.xiaotong6666.fusehide.config.PackageHideRule
import io.github.xiaotong6666.fusehide.config.formatHiddenTargetRules
import io.github.xiaotong6666.fusehide.config.parseHiddenTargetRules

data class GlobalConfigDrafts(
    val visibleExemptionsText: String,
    val publicVisibleExemptionsText: String,
    val hiddenTargetsText: String,
    val hiddenPackagesText: String,
)

fun globalConfigDrafts(config: HideConfig): GlobalConfigDrafts = GlobalConfigDrafts(
    visibleExemptionsText = HideConfigDefaults.toEditorText(config.hideAllRootEntriesExemptions),
    publicVisibleExemptionsText =
    HideConfigDefaults.toEditorText(config.hideAllPublicRootEntriesExemptions),
    hiddenTargetsText = formatHiddenTargetRules(config),
    hiddenPackagesText = HideConfigDefaults.toEditorText(config.hiddenPackages),
)

fun defaultGlobalConfigDrafts(): GlobalConfigDrafts = globalConfigDrafts(HideConfigDefaults.value)

fun appSpecificTargetsText(config: HideConfig, packageName: String): String {
    val specificRule = config.packageRules.find { it.packageName == packageName } ?: return ""
    val rootEntries = specificRule.hiddenRootEntryNames
    val relativePaths = specificRule.hiddenRelativePaths.map { "|$it" }
    return HideConfigDefaults.toEditorText(rootEntries + relativePaths)
}

fun updatedConfigForVisibleExemptions(currentConfig: HideConfig, visibleExemptionsText: String): HideConfig = currentConfig.copy(
    hideAllRootEntriesExemptions = HideConfigDefaults.parseEditorText(visibleExemptionsText),
)

fun updatedConfigForPublicVisibleExemptions(currentConfig: HideConfig, value: String): HideConfig = currentConfig.copy(
    hideAllPublicRootEntriesExemptions = HideConfigDefaults.parseEditorText(value),
)

fun updatedConfigForHiddenTargets(currentConfig: HideConfig, hiddenTargetsText: String): HideConfig {
    val parsed = parseHiddenTargetRules(hiddenTargetsText)
    return currentConfig.copy(
        hiddenRootEntryNames = parsed.hiddenRootEntryNames,
        hiddenRelativePaths = parsed.hiddenRelativePaths,
        packageRules = parsed.packageRules,
    )
}

fun updatedConfigForHiddenPackages(currentConfig: HideConfig, hiddenPackagesText: String): HideConfig = currentConfig.copy(
    hiddenPackages = HideConfigDefaults.parseEditorText(hiddenPackagesText),
)

fun updatedConfigForPackageRule(
    currentConfig: HideConfig,
    packageName: String,
    enabled: Boolean,
    targetsText: String,
): HideConfig {
    val parsedLines = HideConfigDefaults.parseEditorText(targetsText)
    val newHiddenPackages = currentConfig.hiddenPackages.toMutableList()
    val newPackageRules = currentConfig.packageRules.toMutableList()

    newHiddenPackages.remove(packageName)
    newPackageRules.removeAll { it.packageName == packageName }

    if (enabled) {
        if (parsedLines.isEmpty()) {
            newHiddenPackages.add(packageName)
        } else {
            val relativePaths = parsedLines
                .filter { it.startsWith("|") || it.contains('/') }
                .map { it.removePrefix("|") }
            val rootEntries = parsedLines.filterNot { it.startsWith("|") || it.contains('/') }
            newPackageRules.add(
                PackageHideRule(
                    packageName = packageName,
                    hiddenRootEntryNames = rootEntries,
                    hiddenRelativePaths = relativePaths,
                ),
            )
        }
    }

    return currentConfig.copy(
        hiddenPackages = newHiddenPackages,
        packageRules = newPackageRules,
    )
}
