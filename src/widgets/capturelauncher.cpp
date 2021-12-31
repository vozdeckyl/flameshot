// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2018 Alejandro Sirgo Rica & Contributors

#include "capturelauncher.h"
#include "src/core/controller.h"
#include "src/utils/confighandler.h"
#include "src/utils/globalvalues.h"
#include "src/utils/screengrabber.h"
#include "src/utils/screenshotsaver.h"
#include "src/utils/valuehandler.h"
#include "src/widgets/imagelabel.h"
#include <QComboBox>
#include <QDrag>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QSpinBox>

// https://github.com/KDE/spectacle/blob/941c1a517be82bed25d1254ebd735c29b0d2951c/src/Gui/KSWidget.cpp
// https://github.com/KDE/spectacle/blob/941c1a517be82bed25d1254ebd735c29b0d2951c/src/Gui/KSMainWindow.cpp

CaptureLauncher::CaptureLauncher(QDialog* parent)
  : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(GlobalValues::iconPath()));
    m_imageLabel = new ImageLabel(this);
    bool ok;
    m_imageLabel->setScreenshot(ScreenGrabber().grabEntireDesktop(ok));
    if (!ok) {
    }
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_imageLabel,
            &ImageLabel::dragInitiated,
            this,
            &CaptureLauncher::startDrag);

    QGridLayout* layout = new QGridLayout(this);
    layout->addWidget(m_imageLabel, 0, 0);

    m_CaptureModeLabel = new QLabel(tr("<b>Capture Mode</b>"));

    m_captureType = new QComboBox();
    m_captureType->setMinimumWidth(240);
    // TODO remember number
    m_captureType->insertItem(
      1, tr("Rectangular Region"), CaptureRequest::GRAPHICAL_MODE);

#if defined(Q_OS_MACOS)
    // Following to MacOS philosophy (one application cannot be displayed on
    // more than one display)
    m_captureType->insertItem(
      2, tr("Full Screen (Current Display)"), CaptureRequest::FULLSCREEN_MODE);
#else
    m_captureType->insertItem(
      2, tr("Full Screen (All Monitors)"), CaptureRequest::FULLSCREEN_MODE);
#endif

    m_delaySpinBox = new QSpinBox();
    m_delaySpinBox->setSingleStep(1.0);
    m_delaySpinBox->setMinimum(0.0);
    m_delaySpinBox->setMaximum(999.0);
    m_delaySpinBox->setSpecialValueText(tr("No Delay"));
    m_delaySpinBox->setMinimumWidth(160);
    // with QT 5.7 qOverload<int>(&QSpinBox::valueChanged),
    connect(m_delaySpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int val) {
                QString suffix = val == 1 ? tr(" second") : tr(" seconds");
                this->m_delaySpinBox->setSuffix(suffix);
            });

    m_width = new QLineEdit("0");
    m_height = new QLineEdit("0");
    m_xOffset = new QLineEdit("0");
    m_yOffset = new QLineEdit("0");

    setLastRegion();

    auto* dims_layout = new QHBoxLayout();
    dims_layout->addWidget(m_width);
    dims_layout->addWidget(m_height);
    dims_layout->addWidget(m_xOffset);
    dims_layout->addWidget(m_yOffset);

    auto* dims = new QWidget;
    dims->setLayout(dims_layout);

    m_launchButton = new QPushButton(tr("Take new screenshot"));
    m_launchButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_launchButton,
            &QPushButton::clicked,
            this,
            &CaptureLauncher::startCapture);
    m_launchButton->setFocus();

    QFormLayout* captureModeForm = new QFormLayout;
    captureModeForm->addRow(tr("Area:"), m_captureType);
    captureModeForm->addRow(tr("Delay:"), m_delaySpinBox);
    captureModeForm->addRow(tr("WxH+x+y:"), dims);
    captureModeForm->setContentsMargins(24, 0, 0, 0);

    m_mainLayout = new QVBoxLayout();
    m_mainLayout->addStretch(1);
    m_mainLayout->addWidget(m_CaptureModeLabel);
    m_mainLayout->addLayout(captureModeForm);
    m_mainLayout->addStretch(10);
    m_mainLayout->addWidget(m_launchButton, 1, Qt::AlignCenter);
    m_mainLayout->setContentsMargins(10, 0, 0, 10);
    layout->addLayout(m_mainLayout, 0, 1);
    layout->setColumnMinimumWidth(0, 320);
    layout->setColumnMinimumWidth(1, 320);
}

void CaptureLauncher::setLastRegion()
{
    ConfigHandler config;
    auto region_str = config.lastRegion();
    QRect region;

    if (!region_str.isEmpty()) {
        m_initial_rect = Region().value(region_str).toRect();
        m_width->setText(QString::number(m_initial_rect.width()));
        m_height->setText(QString::number(m_initial_rect.height()));
        m_xOffset->setText(QString::number(m_initial_rect.x()));
        m_yOffset->setText(QString::number(m_initial_rect.y()));
    } else {
        m_initial_rect = QRect(0, 0, 0, 0);
        m_width->setText("0");
        m_height->setText("0");
        m_xOffset->setText("0");
        m_yOffset->setText("0");
    }
}

// HACK:
// https://github.com/KDE/spectacle/blob/fa1e780b8bf3df3ac36c410b9ece4ace041f401b/src/Gui/KSMainWindow.cpp#L70
void CaptureLauncher::startCapture()
{
    m_launchButton->setEnabled(false);
    hide();
    auto mode = static_cast<CaptureRequest::CaptureMode>(
      m_captureType->currentData().toInt());
    CaptureRequest req(mode, 600 + m_delaySpinBox->value() * 1000);

    if (m_captureType->currentData() ==
        CaptureRequest::CaptureMode::GRAPHICAL_MODE) {
        req.setInitialSelection(m_initial_rect);
    }

    connectCaptureSlots();
    Controller::getInstance()->requestCapture(req);
}

void CaptureLauncher::startDrag()
{
    QDrag* dragHandler = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    mimeData->setImageData(m_imageLabel->pixmap(Qt::ReturnByValue));
#else
    mimeData->setImageData(m_imageLabel->pixmap());
#endif
    dragHandler->setMimeData(mimeData);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    dragHandler->setPixmap(
      m_imageLabel->pixmap(Qt::ReturnByValue)
        .scaled(
          256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    dragHandler->exec();
#else
    dragHandler->setPixmap(m_imageLabel->pixmap()->scaled(
      256, 256, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    dragHandler->exec();
#endif
}

void CaptureLauncher::connectCaptureSlots()
{
    connect(Controller::getInstance(),
            &Controller::captureTaken,
            this,
            &CaptureLauncher::captureTaken);
    connect(Controller::getInstance(),
            &Controller::captureFailed,
            this,
            &CaptureLauncher::captureFailed);
}

void CaptureLauncher::disconnectCaptureSlots()
{
    // Hack for MacOS
    // for some strange reasons MacOS sends multiple "captureTaken" signals
    // (random number, usually from 1 up to 20).
    // So no it enables signal on "Capture new screenshot" button and disables
    // on first success of fail.
    disconnect(Controller::getInstance(),
               &Controller::captureTaken,
               this,
               &CaptureLauncher::captureTaken);
    disconnect(Controller::getInstance(),
               &Controller::captureFailed,
               this,
               &CaptureLauncher::captureFailed);
}

void CaptureLauncher::captureTaken(QPixmap p, const QRect&)
{
    // MacOS specific, more details in the function disconnectCaptureSlots()
    disconnectCaptureSlots();

    m_imageLabel->setScreenshot(p);
    show();

    auto mode = static_cast<CaptureRequest::CaptureMode>(
      m_captureType->currentData().toInt());

    if (mode == CaptureRequest::FULLSCREEN_MODE) {
        ScreenshotSaver().saveToFilesystemGUI(p);
    } else {
        setLastRegion();
    }
    m_launchButton->setEnabled(true);
}

void CaptureLauncher::captureFailed()
{
    // MacOS specific, more details in the function disconnectCaptureSlots()
    disconnectCaptureSlots();
    show();
    m_launchButton->setEnabled(true);
}
