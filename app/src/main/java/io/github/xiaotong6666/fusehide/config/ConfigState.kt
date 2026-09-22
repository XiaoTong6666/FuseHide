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

package io.github.xiaotong6666.fusehide.config

import android.content.Context
import io.github.xiaotong6666.fusehide.R
import io.github.xiaotong6666.fusehide.ui.core.model.HideConfigDiff
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

data class ParsedHiddenTargetRules(
    val hiddenRootEntryNames: List<String>,
    val hiddenRelativePaths: List<String>,
    val packageRules: List<PackageHideRule>,
)

private val packageSectionRegex = Regex("""^\[([A-Za-z0-9_.]+)]$""")

private fun splitTargetValues(values: List<String>): Pair<List<String>, List<String>> {
    val rootNames = mutableListOf<String>()
    val relativePaths = mutableListOf<String>()
    values.forEach { value ->
        if (value.contains('/')) {
            relativePaths += value
        } else {
            rootNames += value
        }
    }
    return rootNames to relativePaths
}

fun parseHiddenTargetRules(text: String): ParsedHiddenTargetRules {
    val globalValues = mutableListOf<String>()
    val packageValues = linkedMapOf<String, MutableList<String>>()
    var currentPackage: String? = null
    text.lineSequence()
        .map { it.trim() }
        .filter { it.isNotEmpty() && !it.startsWith("#") }
        .forEach { line ->
            val packageName = packageSectionRegex.matchEntire(line)?.groupValues?.get(1)
            if (packageName != null) {
                currentPackage = packageName
                packageValues.getOrPut(packageName) { mutableListOf() }
                return@forEach
            }
            val scopedValues = currentPackage?.let { packageValues.getOrPut(it) { mutableListOf() } }
            if (scopedValues == null) {
                globalValues += line
            } else {
                scopedValues += line
            }
        }

    val (globalRootNames, globalRelativePaths) = splitTargetValues(globalValues.distinct())
    val packageRules = packageValues.mapNotNull { (packageName, values) ->
        val (rootNames, relativePaths) = splitTargetValues(values.distinct())
        if (rootNames.isEmpty() && relativePaths.isEmpty()) {
            null
        } else {
            PackageHideRule(packageName, rootNames, relativePaths)
        }
    }
    return ParsedHiddenTargetRules(globalRootNames, globalRelativePaths, packageRules)
}

fun formatHiddenTargetRules(config: HideConfig): String = buildString {
    val globalTargets = config.hiddenRootEntryNames + config.hiddenRelativePaths
    append(HideConfigDefaults.toEditorText(globalTargets))
    config.packageRules.forEach { rule ->
        if (isNotEmpty()) append("\n\n")
        append("[").append(rule.packageName).append("]\n")
        append(HideConfigDefaults.toEditorText(rule.hiddenRootEntryNames + rule.hiddenRelativePaths))
    }
}

fun buildAppliedConfigSnapshot(config: HideConfig): String = buildString {
    append("MediaProvider current native config\n")
    append("enableHideAllRootEntries=${config.enableHideAllRootEntries}\n")
    append("hideAllRootEntriesExemptions=\n")
    if (config.hideAllRootEntriesExemptions.isEmpty()) append("(empty)\n")
    config.hideAllRootEntriesExemptions.forEach { append("- $it\n") }
    append("hiddenRootEntryNames=\n")
    if (config.hiddenRootEntryNames.isEmpty()) append("(empty)\n")
    config.hiddenRootEntryNames.forEach { append("- $it\n") }
    append("hiddenRelativePaths=\n")
    if (config.hiddenRelativePaths.isEmpty()) append("(empty)\n")
    config.hiddenRelativePaths.forEach { append("- $it\n") }
    append("hiddenPackages=\n")
    if (config.hiddenPackages.isEmpty()) append("(empty)\n")
    config.hiddenPackages.forEach { append("- $it\n") }
    append("packageRules=\n")
    if (config.packageRules.isEmpty()) append("(empty)\n")
    config.packageRules.forEach { rule ->
        append("[").append(rule.packageName).append("]\n")
        rule.hiddenRootEntryNames.forEach { append("- ").append(it).append("\n") }
        rule.hiddenRelativePaths.forEach { append("- ").append(it).append("\n") }
    }
}

fun buildDraftVsAppliedDiff(context: Context, draft: HideConfig, applied: HideConfig?): HideConfigDiff {
    if (applied == null) {
        return HideConfigDiff(
            hasDifferences = false,
            summary = context.getString(R.string.diff_summary_missing),
            details = context.getString(R.string.label_draft_vs_applied) + "\n" + context.getString(R.string.diff_summary_missing) + "\n",
        )
    }

    fun section(title: String, draftValues: List<String>, appliedValues: List<String>): String {
        val draftSet = draftValues.toSet()
        val appliedSet = appliedValues.toSet()
        val draftOnly = (draftSet - appliedSet).sorted()
        val appliedOnly = (appliedSet - draftSet).sorted()
        val shared = (draftSet intersect appliedSet).sorted()
        return buildString {
            append(title).append("\n")
            append("draftOnly=\n")
            if (draftOnly.isEmpty()) append("(empty)\n")
            draftOnly.forEach { append("- ").append(it).append("\n") }
            append("appliedOnly=\n")
            if (appliedOnly.isEmpty()) append("(empty)\n")
            appliedOnly.forEach { append("- ").append(it).append("\n") }
            append("shared=\n")
            if (shared.isEmpty()) append("(empty)\n")
            shared.forEach { append("- ").append(it).append("\n") }
        }
    }

    val boolMatches = draft.enableHideAllRootEntries == applied.enableHideAllRootEntries
    val details = buildString {
        append("Draft vs applied diff\n")
        append("enableHideAllRootEntries: ")
        append(if (boolMatches) "MATCH" else "DIFF")
        append(" (draft=").append(draft.enableHideAllRootEntries)
        append(", applied=").append(applied.enableHideAllRootEntries).append(")\n\n")
        append(section("hideAllRootEntriesExemptions", draft.hideAllRootEntriesExemptions, applied.hideAllRootEntriesExemptions)).append("\n")
        append(section("hiddenRootEntryNames", draft.hiddenRootEntryNames, applied.hiddenRootEntryNames)).append("\n")
        append(section("hiddenRelativePaths", draft.hiddenRelativePaths, applied.hiddenRelativePaths)).append("\n")
        append(section("hiddenPackages", draft.hiddenPackages, applied.hiddenPackages)).append("\n")
        append(section("packageRules", draft.packageRules.map { it.toString() }, applied.packageRules.map { it.toString() }))
    }
    val hasDifferences = hasDraftVsAppliedDifferences(draft, applied)
    val summary = if (hasDifferences) {
        context.getString(R.string.diff_summary_mismatch)
    } else {
        context.getString(R.string.diff_summary_match)
    }
    return HideConfigDiff(hasDifferences = hasDifferences, summary = summary, details = details)
}

fun hasDraftVsAppliedDifferences(draft: HideConfig, applied: HideConfig?): Boolean {
    if (applied == null) return false
    return draft.enableHideAllRootEntries != applied.enableHideAllRootEntries ||
        draft.hideAllRootEntriesExemptions.toSet() != applied.hideAllRootEntriesExemptions.toSet() ||
        draft.hiddenRootEntryNames.toSet() != applied.hiddenRootEntryNames.toSet() ||
        draft.hiddenRelativePaths.toSet() != applied.hiddenRelativePaths.toSet() ||
        draft.hiddenPackages.toSet() != applied.hiddenPackages.toSet() ||
        draft.packageRules.toSet() != applied.packageRules.toSet()
}

fun formatNow(): String = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
