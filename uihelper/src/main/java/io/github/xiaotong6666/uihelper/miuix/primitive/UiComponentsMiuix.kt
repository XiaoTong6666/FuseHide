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

@file:Suppress("ktlint:standard:function-naming")

package io.github.xiaotong6666.uihelper.miuix.primitive

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.state.ToggleableState
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.xiaotong6666.uihelper.model.GridActionItem
import io.github.xiaotong6666.uihelper.model.SectionDescriptionStyle
import io.github.xiaotong6666.uihelper.model.SectionTitleStyle
import top.yukonga.miuix.kmp.basic.Button
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.CardDefaults
import top.yukonga.miuix.kmp.basic.Checkbox
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.basic.TextButton
import top.yukonga.miuix.kmp.basic.TextField
import top.yukonga.miuix.kmp.basic.TextFieldDefaults
import top.yukonga.miuix.kmp.theme.MiuixTheme

@Composable
fun SectionTitleMiuix(
    text: String,
    style: SectionTitleStyle,
) {
    Text(
        text = text,
        style = when (style) {
            SectionTitleStyle.Large -> MiuixTheme.textStyles.title3.copy(fontWeight = FontWeight.Medium)
            SectionTitleStyle.Medium -> MiuixTheme.textStyles.title4
            SectionTitleStyle.EmphasizedMedium -> MiuixTheme.textStyles.title4.copy(fontWeight = FontWeight.Medium)
            SectionTitleStyle.Small -> MiuixTheme.textStyles.title4
            SectionTitleStyle.Label -> MiuixTheme.textStyles.footnote1.copy(fontWeight = FontWeight.Medium)
            SectionTitleStyle.Subsection -> MiuixTheme.textStyles.title4.copy(fontWeight = FontWeight.Medium)
        },
        color = if (style == SectionTitleStyle.Label) MiuixTheme.colorScheme.onSurfaceVariantSummary else MiuixTheme.colorScheme.onSurface,
    )
}

@Composable
fun SectionDescriptionMiuix(
    text: String,
    style: SectionDescriptionStyle,
) {
    Text(
        text = text,
        style = when (style) {
            SectionDescriptionStyle.Body -> MiuixTheme.textStyles.body1
            SectionDescriptionStyle.Supporting -> MiuixTheme.textStyles.footnote1
        },
        color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
    )
}

@Composable
fun AppTextFieldMiuix(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    singleLine: Boolean = false,
) {
    TextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier,
        label = label,
        colors = TextFieldDefaults.textFieldColors(
            backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
            labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            borderColor = MiuixTheme.colorScheme.primary,
        ),
        textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurface),
        singleLine = singleLine,
    )
}

@Composable
fun ConfigTextFieldMiuix(
    value: String,
    onValueChange: (String) -> Unit,
    label: String,
    modifier: Modifier = Modifier,
    minLines: Int = 1,
    maxLines: Int = Int.MAX_VALUE,
) {
    TextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier,
        label = label,
        colors = TextFieldDefaults.textFieldColors(
            backgroundColor = MiuixTheme.colorScheme.surfaceContainerHighest,
            labelColor = MiuixTheme.colorScheme.onSurfaceVariantSummary,
            borderColor = MiuixTheme.colorScheme.primary,
        ),
        textStyle = MiuixTheme.textStyles.main.copy(color = MiuixTheme.colorScheme.onSurfaceSecondary),
        minLines = minLines,
        maxLines = maxLines,
    )
}

@Composable
fun ConfigToggleCardMiuix(
    checked: Boolean,
    title: String,
    description: String,
    onToggle: () -> Unit,
) {
    Card(
        onClick = onToggle,
        colors = CardDefaults.defaultColors(
            color = MiuixTheme.colorScheme.surfaceContainerHighest,
            contentColor = MiuixTheme.colorScheme.onSurfaceContainerHighest,
        ),
        insideMargin = PaddingValues(0.dp),
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(14.dp),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
            verticalAlignment = Alignment.Top,
        ) {
            Checkbox(
                state = if (checked) ToggleableState.On else ToggleableState.Off,
                onClick = null,
            )
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(title, style = MiuixTheme.textStyles.headline2)
                Text(
                    text = description,
                    style = MiuixTheme.textStyles.footnote1,
                    color = MiuixTheme.colorScheme.onSurfaceVariantSummary,
                )
            }
        }
    }
}

@Composable
fun InlineTextButtonMiuix(
    label: String,
    onClick: () -> Unit,
) {
    TextButton(
        text = label,
        onClick = onClick,
    )
}

@Composable
fun DualActionRowMiuix(
    primaryLabel: String,
    onPrimaryClick: () -> Unit,
    secondaryLabel: String,
    onSecondaryClick: () -> Unit,
    primaryFilled: Boolean = true,
) {
    val hasPrimary = primaryLabel.isNotEmpty()
    val hasSecondary = secondaryLabel.isNotEmpty()
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = if (hasPrimary && hasSecondary) Arrangement.spacedBy(10.dp) else Arrangement.Start,
    ) {
        val weight = if (hasPrimary && hasSecondary) Modifier.weight(1f) else Modifier.fillMaxWidth()
        if (hasPrimary) {
            if (primaryFilled) {
                Button(
                    onClick = onPrimaryClick,
                    modifier = weight.height(56.dp),
                    colors = ButtonDefaults.buttonColorsPrimary(),
                ) {
                    Text(primaryLabel, maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium))
                }
            } else {
                Button(
                    onClick = onPrimaryClick,
                    modifier = weight.height(56.dp),
                    colors = ButtonDefaults.buttonColors().copy(color = MiuixTheme.colorScheme.secondary),
                ) {
                    Text(primaryLabel, maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium))
                }
            }
        }
        if (hasSecondary) {
            Button(
                onClick = onSecondaryClick,
                modifier = weight.height(56.dp),
                colors = ButtonDefaults.buttonColors().copy(color = MiuixTheme.colorScheme.secondary),
            ) {
                Text(secondaryLabel, maxLines = 1, overflow = TextOverflow.Ellipsis, textAlign = TextAlign.Center, style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium))
            }
        }
    }
}

@Composable
fun PrimaryActionButtonMiuix(
    label: String,
    onClick: () -> Unit,
) {
    Button(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().height(56.dp),
        colors = ButtonDefaults.buttonColorsPrimary(),
    ) {
        Text(
            text = label,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            textAlign = TextAlign.Center,
            style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium),
        )
    }
}

@Composable
fun ActionGridMiuix(actions: List<GridActionItem>) {
    actions.chunked(2).forEach { rowActions ->
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            rowActions.forEach { item ->
                val currentBgColor = if (item.isError) MiuixTheme.colorScheme.error else MiuixTheme.colorScheme.secondary
                Button(
                    onClick = item.action,
                    modifier = Modifier.weight(1f).height(56.dp),
                    colors = ButtonDefaults.buttonColors().copy(color = currentBgColor),
                    insideMargin = PaddingValues(horizontal = 12.dp, vertical = 14.dp),
                ) {
                    Text(
                        text = item.label,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        textAlign = TextAlign.Center,
                        color = if (item.isError) MiuixTheme.colorScheme.onError else Color.Unspecified,
                        style = MiuixTheme.textStyles.button.copy(fontWeight = FontWeight.Medium),
                    )
                }
            }
            if (rowActions.size == 1) {
                Spacer(modifier = Modifier.weight(1f))
            }
        }
        Spacer(Modifier.height(10.dp))
    }
}

@Composable
fun SectionCardMiuix(content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        content = content,
    )
}

@Composable
fun WarningBannerMiuix(
    message: String,
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
) {
    val colors = CardDefaults.defaultColors(
        color = MiuixTheme.colorScheme.errorContainer,
        contentColor = MiuixTheme.colorScheme.onErrorContainer,
    )
    val content: @Composable () -> Unit = {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = message,
                color = MiuixTheme.colorScheme.onErrorContainer,
                fontSize = 14.sp,
            )
        }
    }
    if (onClick != null) {
        Card(
            modifier = modifier.fillMaxWidth(),
            onClick = onClick,
            colors = colors,
            showIndication = true,
            insideMargin = PaddingValues(0.dp),
        ) { content() }
    } else {
        Card(
            modifier = modifier.fillMaxWidth(),
            colors = colors,
            insideMargin = PaddingValues(0.dp),
        ) { content() }
    }
}

@Composable
fun InfoBannerMiuix(
    message: String,
    modifier: Modifier = Modifier,
    onClick: (() -> Unit)? = null,
) {
    val colors = CardDefaults.defaultColors(
        color = MiuixTheme.colorScheme.primaryContainer.copy(alpha = 0.25f),
        contentColor = MiuixTheme.colorScheme.primary,
    )
    val content: @Composable () -> Unit = {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 16.dp, end = 16.dp, top = 16.dp, bottom = 16.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = message,
                style = MiuixTheme.textStyles.body2,
                color = MiuixTheme.colorScheme.primary,
                fontWeight = FontWeight.SemiBold,
            )
        }
    }
    if (onClick != null) {
        Card(
            modifier = modifier.fillMaxWidth(),
            onClick = onClick,
            colors = colors,
            showIndication = true,
            insideMargin = PaddingValues(0.dp),
        ) { content() }
    } else {
        Card(
            modifier = modifier.fillMaxWidth(),
            colors = colors,
            insideMargin = PaddingValues(0.dp),
        ) { content() }
    }
}
