// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "capturecontext.h"
#include "capturerequest.h"
#include "flameshot.h"

// TODO rename
QPixmap CaptureContext::selectedScreenshotArea() const
{
    if (selection.isNull()) {
        return screenshot;
    } else {
        if (grayedOutScreenshot) {
            // transfer to grayscale
            QImage img = screenshot.toImage();
            for (int i = 0; i < img.width(); i++) {
                for (int j = 0; j < img.height(); j++) {
                    QRgb p = img.pixel(i, j);
                    int grayColor = qGray(p);
                    // decrease the contrast
                    grayColor = 100 + (int)50 * ((float)grayColor / 255.0);

                    img.setPixelColor(
                      i, j, QColor(grayColor, grayColor, grayColor));
                }
            }

            // copy the
            QPixmap selectedPixmap = screenshot.copy(selection);
            QPainter painter(&img);
            painter.drawImage(selection, selectedPixmap.toImage());

            return QPixmap::fromImage(img);
            // WIP
        } else {
            return screenshot.copy(selection);
        }
    }
}
