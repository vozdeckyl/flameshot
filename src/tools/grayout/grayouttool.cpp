// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "grayouttool.h"
#include <QPainter>
#include <iostream>

GrayoutTool::GrayoutTool(QObject* parent)
  : AbstractActionTool(parent)
{}

bool GrayoutTool::closeOnButtonPressed() const
{
    return true;
}

QIcon GrayoutTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "content-save-grayedout.svg");
}
QString GrayoutTool::name() const
{
    return tr("Save grayedout");
}

CaptureTool::Type GrayoutTool::type() const
{
    return CaptureTool::TYPE_GRAYOUT;
}

QString GrayoutTool::description() const
{
    return tr("Save the selection with grayed out surroundings.");
}

CaptureTool* GrayoutTool::copy(QObject* parent)
{
    return new GrayoutTool(parent);
}

void GrayoutTool::pressed(CaptureContext& context)
{
    context.grayedOutScreenshot = true;
    emit requestAction(REQ_CLEAR_SELECTION);
    context.request.addSaveTask();
    emit requestAction(REQ_CAPTURE_DONE_OK);
    emit requestAction(REQ_CLOSE_GUI);
}
