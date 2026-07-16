#include "gui.hpp" // Include the header file
// gui.cpp
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QScreen>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QCursor>
#include <QRegion>
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QStringList>
#include <QShortcut>
#include <QStatusBar>
#include <QFrame>
#include <QPainterPath>
#include <QRadialGradient>
#include <QGraphicsDropShadowEffect>

#include <opencv2/opencv.hpp>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────
//  Style Constants (Warm Espresso Theme)
// ─────────────────────────────────────────────
static const char* kAppQSS = R"(
    QMainWindow, QWidget#CentralWidget { background-color: #161312; }
    
    QWidget#HeaderWidget {
        background-color: #242020;
        border-bottom: 2px solid #5c4a42;
    }
    QWidget#FooterWidget {
        background-color: #242020;
        border-top: 2px solid #5c4a42;
    }

    QLabel { color: #f0e6d2; }

    QLabel#AppTitle {
        font-size: 20px;
        font-weight: bold;
        color: #ffffff;
        padding: 2px 0px;
    }
    QLabel#AppSubtitle {
        font-size: 12px;
        color: #a89888;
        padding-bottom: 4px;
    }
    QLabel#FilePathLabel {
        font-size: 13px;
        font-weight: bold;
        color: #d9a05b;
        padding-bottom: 6px;
    }

    QPushButton {
        background-color: #3a322c;
        color: #f0e6d2;
        border: 1px solid #5c4a42;
        border-radius: 6px;
        padding: 8px 18px;
        font-size: 13px;
        font-weight: 600;
        min-width: 80px;
    }
    QPushButton:hover {
        background-color: #4a3f37;
        border-color: #8c7a6e;
        color: #ffffff;
    }
    QPushButton:pressed {
        background-color: #2a2522;
    }

    QPushButton#PrimaryBtn {
        background-color: #3a6b38;
        border: 1px solid #4d8a4a;
        color: #ffffff;
    }
    QPushButton#PrimaryBtn:hover {
        background-color: #4d8a4a;
        border-color: #63a660;
    }
    QPushButton#PrimaryBtn:pressed {
        background-color: #2d572c;
    }

    QPushButton#DangerBtn {
        background-color: #3a322c;
        border: 1px solid #8a2d2d;
        color: #e57373;
    }
    QPushButton#DangerBtn:hover {
        background-color: #4a2a2a;
        border-color: #b34141;
        color: #ff8a8a;
    }

    QStatusBar {
        background-color: #161312;
        color: #a89888;
        border-top: 1px solid #2a2522;
        font-size: 12px;
    }
    QStatusBar::item { border: none; }
)";

// ─────────────────────────────────────────────
//  Utility functions  (unchanged)
// ─────────────────────────────────────────────
QSize getMonitorResolution() {
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect geometry = screen->availableGeometry();
        return QSize(geometry.width(), geometry.height());
    }
    return QSize(1920, 1080);
}

QSize getVideoResolution(const QString& path) {
    cv::VideoCapture cap(path.toStdString());
    if (!cap.isOpened()) {
        qDebug() << "Error: Failed to open video" << path;
        return QSize(640, 480);
    }
    int width  = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    cap.release();
    return QSize(width, height);
}

QString getConfigPath() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return configDir + "/HyprLarp.json";
}

// ═════════════════════════════════════════════
//  VideoArea Implementation
// ═════════════════════════════════════════════
VideoArea::VideoArea(QSize monitorRatio_, QSize videoRatio_, QWidget* parent)
    : QWidget(parent),
      monitorRatio(monitorRatio_),
      videoRatio(videoRatio_),
      bgColor(QColor(255, 0, 0, 25)),
      borderColor(QColor(255, 0, 0)),
      handleColor(QColor(255, 255, 0)),
      handleRadius(5),
      handleRadiusHover(8),
      dragging(false),
      resizing(false),
      currentScale(1.0),
      realRect(0, 0, 0, 0)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
}

void VideoArea::updateRealGeometry() {
    if (!parent()) return;
    WorkspaceArea* ws = static_cast<WorkspaceArea*>(parent());
    QRect wsRect = ws->currentWorkspaceRect;
    int mw = ws->displayRatio.width();
    int mh = ws->displayRatio.height();

    if (wsRect.width() <= 0 || wsRect.height() <= 0) return;

    QRect vRect = geometry();

    double rx = (vRect.x() - wsRect.x()) / (double)wsRect.width() * mw;
    double ry = (vRect.y() - wsRect.y()) / (double)wsRect.height() * mh;
    double rw = vRect.width()  / (double)wsRect.width()  * mw;
    double rh = vRect.height() / (double)wsRect.height() * mh;

    realRect = QRect((int)std::round(rx), (int)std::round(ry),
                     (int)std::round(rw), (int)std::round(rh));
}

QRect VideoArea::getCornerRect(const QString& name) const {
    QRect r = rect();
    int size = (!hoveredCorner.isEmpty()) ? handleRadiusHover : handleRadius;
    if (name == "TL") return QRect(r.topLeft()     - QPoint(size, size),  QSize(size * 2, size * 2));
    if (name == "TR") return QRect(r.topRight()    - QPoint(-size, size), QSize(size * 2, size * 2));
    if (name == "BL") return QRect(r.bottomLeft()  - QPoint(size, -size), QSize(size * 2, size * 2));
    if (name == "BR") return QRect(r.bottomRight() - QPoint(-size, -size),QSize(size * 2, size * 2));
    return QRect();
}

QPoint VideoArea::getCornerCenter(const QString& name) const {
    QRect r = rect();
    if (name == "TL") return r.topLeft();
    if (name == "TR") return r.topRight();
    if (name == "BL") return r.bottomLeft();
    if (name == "BR") return r.bottomRight();
    return QPoint();
}

QString VideoArea::checkCornerHover(const QPoint& pos) const {
    QStringList corners = {"TL", "TR", "BL", "BR"};
    for (const QString& name : corners) {
        QPoint center = getCornerCenter(name);
        QPoint diff = pos - center;
        int dist = diff.manhattanLength();
        if (dist <= handleRadiusHover * 1.5) {
            return name;
        }
    }
    return QString();
}

void VideoArea::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // ── Fill ──
    painter.fillRect(rect(), QBrush(bgColor));

    // ── Crosshair guides during resize ──
    if (resizing && !activeCorner.isEmpty()) {
        QPoint ac = getCornerCenter(activeCorner);
        painter.setPen(QPen(QColor(255, 255, 0, 70), 1, Qt::DashLine));
        painter.drawLine(0, ac.y(), width(), ac.y());
        painter.drawLine(ac.x(), 0, ac.x(), height());
    }

    // ── Center-alignment guides during drag ──
    if (dragging && parent()) {
        WorkspaceArea* ws = static_cast<WorkspaceArea*>(parent());
        QPoint wsCenter = mapFromParent(ws->currentWorkspaceRect.center());
        QRect  r = rect();
        if (std::abs(r.center().x() - wsCenter.x()) <= 6) {
            painter.setPen(QPen(QColor(0, 255, 120, 160), 2));
            painter.drawLine(wsCenter.x(), 0, wsCenter.x(), height());
        }
        if (std::abs(r.center().y() - wsCenter.y()) <= 6) {
            painter.setPen(QPen(QColor(0, 255, 120, 160), 2));
            painter.drawLine(0, wsCenter.y(), width(), wsCenter.y());
        }
    }

    // ── Border ──
    painter.setPen(QPen(borderColor, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    // ── Dimension badge during interaction ──
    if (dragging || resizing) {
        QFont badgeFont("Segoe UI", 9);
        badgeFont.setBold(true);
        painter.setFont(badgeFont);
        QString info = QString("%1×%2  (×%3)")
            .arg(realRect.width()).arg(realRect.height())
            .arg(currentScale, 0, 'f', 2);
        QFontMetrics fm(badgeFont);
        int tw = fm.horizontalAdvance(info) + 14;
        int th = fm.height() + 6;
        QRect badge(QPoint(4, 4), QSize(tw, th));
        painter.setBrush(QBrush(QColor(0, 0, 0, 210)));
        painter.setPen(QPen(QColor(255, 255, 0, 120), 1));
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(QColor(255, 255, 0));
        painter.drawText(badge, Qt::AlignCenter, info);
    }

    // ── Corner handles with glow ──
    QStringList corners = {"TL", "TR", "BL", "BR"};
    for (const QString& name : corners) {
        bool active = (hoveredCorner == name) || (activeCorner == name);
        int radius  = active ? handleRadiusHover : handleRadius;
        QPoint center = getCornerCenter(name);

        // Glow halo
        if (active) {
            QRadialGradient glow(center, radius * 2.5);
            glow.setColorAt(0.0, QColor(255, 255, 0, 90));
            glow.setColorAt(1.0, QColor(255, 255, 0, 0));
            painter.setBrush(QBrush(glow));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(center, (int)(radius * 2.5), (int)(radius * 2.5));
        }

        // Handle body
        painter.setBrush(QBrush(handleColor));
        painter.setPen(QPen(QColor(0, 0, 0, 200), 1));
        painter.drawEllipse(center, radius, radius);

        // Inner dot
        painter.setBrush(QBrush(QColor(180, 140, 0)));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(center, radius / 3, radius / 3);
    }
}

void VideoArea::mouseMoveEvent(QMouseEvent* event) {
    QPoint pos = event->position().toPoint();

    if (resizing && !activeCorner.isEmpty()) {
        performResize(pos);
        if (activeCorner == "TL" || activeCorner == "BR") {
            setCursor(Qt::SizeFDiagCursor);
        } else {
            setCursor(Qt::SizeBDiagCursor);
        }
    } else if (dragging) {
        setCursor(Qt::ClosedHandCursor);
        QPoint globalPos = event->globalPosition().toPoint();
        QPoint delta = globalPos - startPos;
        move(widgetStartGeo.topLeft() + delta);
    } else {
        QString corner = checkCornerHover(pos);
        if (corner != hoveredCorner) {
            hoveredCorner = corner;
            update();
        }

        if (!corner.isEmpty()) {
            if (corner == "TL" || corner == "BR") {
                setCursor(Qt::SizeFDiagCursor);
            } else {
                setCursor(Qt::SizeBDiagCursor);
            }
        } else {
            setCursor(Qt::OpenHandCursor);
        }
    }
}

void VideoArea::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->position().toPoint();
        QString corner = checkCornerHover(pos);

        if (!corner.isEmpty()) {
            resizing = true;
            activeCorner = corner;
            startPos = event->globalPosition().toPoint();
            widgetStartGeo = geometry();
            grabMouse();
        } else {
            dragging = true;
            startPos = event->globalPosition().toPoint();
            widgetStartGeo = geometry();
            grabMouse();
            setCursor(Qt::ClosedHandCursor);
        }

        update();
    }
}

void VideoArea::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        releaseMouse();
        dragging = false;
        resizing = false;
        activeCorner.clear();

        updateRealGeometry();

        if (parent()) {
            static_cast<WorkspaceArea*>(parent())->updateDebugInfo();
        }
        update();
    }
}

// ── Calculation: UNCHANGED ──
void VideoArea::performResize(const QPoint& currentPos) {
    QRect currentGeo = geometry();
    QPoint center = currentGeo.center();

    QPoint mousePos = mapToParent(currentPos);
    int dx = std::abs(mousePos.x() - center.x());
    int dy = std::abs(mousePos.y() - center.y());

    int vw = videoRatio.width();
    int vh = videoRatio.height();

    int w_from_dx = dx * 2;
    int h_from_dy = dy * 2;

    int new_w = 0, new_h = 0;
    if ((double)w_from_dx * vh >= (double)h_from_dy * vw) {
        new_w = w_from_dx;
        new_h = (int)(new_w * (double)vh / vw);
    } else {
        new_h = h_from_dy;
        new_w = (int)(new_h * (double)vw / vh);
    }

    new_w = std::max(20, new_w);
    new_h = std::max(20, new_h);

    int baseW = 0;
    if (parent()) {
        baseW = static_cast<WorkspaceArea*>(parent())->videoBaseSize.width();
    }
    if (baseW > 0) {
        currentScale = new_w / (double)baseW;
    }

    QRect newGeo(0, 0, new_w, new_h);
    newGeo.moveCenter(center);
    setGeometry(newGeo);

    updateRealGeometry();

    if (parent()) {
        static_cast<WorkspaceArea*>(parent())->updateDebugInfo();
    }
}

void VideoArea::moveEvent(QMoveEvent*) {
    if (parent()) {
        static_cast<WorkspaceArea*>(parent())->updateVideoMask();
    }
}

void VideoArea::resizeEvent(QResizeEvent*) {
    if (parent()) {
        static_cast<WorkspaceArea*>(parent())->updateVideoMask();
    }
}

void VideoArea::resetGeometry(const QRect& rect) {
    setGeometry(rect);
    currentScale = 1.0;
    updateRealGeometry();
    update();
}

// ═════════════════════════════════════════════
//  WorkspaceArea Implementation
// ═════════════════════════════════════════════
WorkspaceArea::WorkspaceArea(QSize displayRatio_, QSize videoRatio_, QWidget* parent)
    : QWidget(parent),
      displayRatio(displayRatio_),
      videoRatio(videoRatio_),
      videoArea(nullptr),
      debugLabel(nullptr),
      noVideoLabel(nullptr),
      currentWorkspaceRect(0, 0, 0, 0),
      videoBaseSize(0, 0)
{
    setMinimumSize(200, 150);
    setObjectName("WorkspaceArea");
}

void WorkspaceArea::resizeEvent(QResizeEvent*) {
    updateLayout();
}

// ── Layout calculation: UNCHANGED ──
void WorkspaceArea::updateLayout() {
    int parentW = width();
    int parentH = height();

    if (parentW <= 0 || parentH <= 0) return;

    int mw = displayRatio.width();
    int mh = displayRatio.height();
    double monitorAspect = (double)mw / mh;

    double maxW = parentW * 0.9;
    double maxH = parentH * 0.9;

    int targetW = (int)maxW;
    int targetH = (int)(targetW / monitorAspect);

    if (targetH > maxH) {
        targetH = (int)maxH;
        targetW = (int)(targetH * monitorAspect);
    }

    targetW = std::max(1, targetW);
    targetH = std::max(1, targetH);

    currentWorkspaceRect = QRect(0, 0, targetW, targetH);
    currentWorkspaceRect.moveCenter(rect().center());

    int vw = videoRatio.width();
    int vh = videoRatio.height();
    double scale = (double)targetW / mw;

    int baseW = (int)(vw * scale);
    int baseH = (int)(vh * scale);
    videoBaseSize = QSize(baseW, baseH);

    if (noVideoLabel) {
        noVideoLabel->setGeometry(currentWorkspaceRect);
    }

    if (!videoArea) {
        update();
        return;
    }

    if (videoArea->realRect.width() > 0) {
        QRect ws = currentWorkspaceRect;
        QRect realR = videoArea->realRect;

        int wx = (int)((realR.x() / (double)mw) * ws.width() + ws.x());
        int wy = (int)((realR.y() / (double)mh) * ws.height() + ws.y());
        int ww = (int)((realR.width()  / (double)mw) * ws.width());
        int wh = (int)((realR.height() / (double)mh) * ws.height());

        QRect vRect(wx, wy, std::max(20, ww), std::max(20, wh));
        videoArea->setGeometry(vRect);

        if (vw > 0) {
            videoArea->currentScale = realR.width() / (double)vw;
        }
    } else {
        QRect videoGeo(0, 0, baseW, baseH);
        videoGeo.moveCenter(currentWorkspaceRect.center());
        videoArea->setGeometry(videoGeo);
        videoArea->currentScale = 1.0;
        videoArea->updateRealGeometry();
    }

    updateDebugInfo();
    updateVideoMask();
}

void WorkspaceArea::updateVideoMask() {
    if (!videoArea) return;

    QRect videoRect = videoArea->geometry();
    QRect intersected = videoRect.intersected(currentWorkspaceRect);

    if (intersected.width() <= 0 || intersected.height() <= 0) {
        videoArea->setMask(QRegion(0, 0, 0, 0));
    } else {
        int localX = intersected.x() - videoRect.x();
        int localY = intersected.y() - videoRect.y();
        QRect localRect(localX, localY, intersected.width(), intersected.height());
        videoArea->setMask(QRegion(localRect));
    }
}

void WorkspaceArea::resetVideo() {
    if (videoArea) {
        videoArea->realRect = QRect(0, 0, 0, 0);
        updateLayout();
    }
}

void WorkspaceArea::updateDebugInfo() {
    if (debugLabel && videoArea) {
        int vw = videoRatio.width();
        int vh = videoRatio.height();
        double scale = videoArea->currentScale;

        QRect rr = videoArea->realRect;
        QString msg = QString(
            "Monitor: %1×%2    Video: %3×%4    Scale: ×%5\n"
            "Real pos: %6, %7    Real size: %8×%9")
            .arg(displayRatio.width()).arg(displayRatio.height())
            .arg(vw).arg(vh)
            .arg(scale, 0, 'f', 2)
            .arg(rr.x()).arg(rr.y())
            .arg(rr.width()).arg(rr.height());

        debugLabel->setText(msg);
        debugLabel->adjustSize();
        debugLabel->move(8, height() - debugLabel->height() - 8);
    }

    // ── Status bar update ──
    QWidget* p = parentWidget();
    while (p) {
        QMainWindow* mw = qobject_cast<QMainWindow*>(p);
        if (mw && mw->statusBar()) {
            if (videoArea) {
                mw->statusBar()->showMessage(QString("Video: %1×%2   |   Scale: ×%3   |   Real: %4,%5  %6×%7")
                    .arg(videoRatio.width()).arg(videoRatio.height())
                    .arg(videoArea->currentScale, 0, 'f', 2)
                    .arg(videoArea->realRect.x()).arg(videoArea->realRect.y())
                    .arg(videoArea->realRect.width()).arg(videoArea->realRect.height()));
            } else {
                mw->statusBar()->showMessage("No video loaded — press Ctrl+O to open one.");
            }
            break;
        }
        p = p->parentWidget();
    }
}

void WorkspaceArea::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // ── Workspace background ──
    painter.fillRect(currentWorkspaceRect, QBrush(QColor(0, 255, 0, 12)));

    // ── Grid lines (10×10) ──
    painter.setPen(QPen(QColor(0, 255, 0, 25), 1));
    int steps = 10;
    for (int i = 1; i < steps; i++) {
        int x = currentWorkspaceRect.x() + (currentWorkspaceRect.width()  * i) / steps;
        int y = currentWorkspaceRect.y() + (currentWorkspaceRect.height() * i) / steps;
        painter.drawLine(x, currentWorkspaceRect.top(),    x, currentWorkspaceRect.bottom());
        painter.drawLine(currentWorkspaceRect.left(),  y, currentWorkspaceRect.right(), y);
    }

    // ── Center crosshair ──
    painter.setPen(QPen(QColor(0, 255, 0, 50), 1, Qt::DashLine));
    QPoint c = currentWorkspaceRect.center();
    painter.drawLine(currentWorkspaceRect.left(),  c.y(), currentWorkspaceRect.right(), c.y());
    painter.drawLine(c.x(), currentWorkspaceRect.top(),   c.x(), currentWorkspaceRect.bottom());

    // ── Border ──
    painter.setPen(QPen(QColor(0, 255, 0, 180), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(currentWorkspaceRect);

    // ── Corner ticks on workspace ──
    painter.setPen(QPen(QColor(0, 255, 0, 200), 3));
    int tick = 12;
    QRect wr = currentWorkspaceRect;
    // TL
    painter.drawLine(wr.topLeft(),     wr.topLeft()     + QPoint(tick, 0));
    painter.drawLine(wr.topLeft(),     wr.topLeft()     + QPoint(0, tick));
    // TR
    painter.drawLine(wr.topRight(),    wr.topRight()    + QPoint(-tick, 0));
    painter.drawLine(wr.topRight(),    wr.topRight()    + QPoint(0, tick));
    // BL
    painter.drawLine(wr.bottomLeft(),  wr.bottomLeft()  + QPoint(tick, 0));
    painter.drawLine(wr.bottomLeft(),  wr.bottomLeft()  + QPoint(0, -tick));
    // BR
    painter.drawLine(wr.bottomRight(), wr.bottomRight() + QPoint(-tick, 0));
    painter.drawLine(wr.bottomRight(), wr.bottomRight() + QPoint(0, -tick));

    // ── Monitor dimension label ──
    QFont labelFont("Segoe UI", 9);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    QString monLabel = QString("Monitor  %1×%2").arg(displayRatio.width()).arg(displayRatio.height());
    QFontMetrics fm(labelFont);
    int lw = fm.horizontalAdvance(monLabel) + 14;
    int lh = fm.height() + 4;
    QRect lblRect(QPoint(wr.x() + 6, wr.y() + 6), QSize(lw, lh));
    painter.setBrush(QBrush(QColor(0, 0, 0, 200)));
    painter.setPen(QPen(QColor(0, 255, 0, 80), 1));
    painter.drawRoundedRect(lblRect, 4, 4);
    painter.setPen(QColor(0, 255, 0));
    painter.drawText(lblRect, Qt::AlignCenter, monLabel);
}

void WorkspaceArea::initUi() {
    // ── Empty-state label ──
    noVideoLabel = new QLabel(this);
    noVideoLabel->setAlignment(Qt::AlignCenter);
    noVideoLabel->setTextFormat(Qt::RichText);
    noVideoLabel->setText(
        "<div style='text-align:center;'>"
        "<div style='font-size:22px; font-weight:bold; color:#6e7681; margin-bottom:8px;'>No Video Loaded</div>"
        "<div style='font-size:13px; color:#484f58;'>Click <b>Open Video</b> or press <b>Ctrl+O</b> to begin</div>"
        "</div>"
    );
    noVideoLabel->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(22, 19, 18, 220);"
        "  border: 2px dashed #4a3f37;"
        "  border-radius: 10px;"
        "  padding: 30px;"
        "}"
    );
    noVideoLabel->setVisible(false);

    // ── Debug info panel ──
    debugLabel = new QLabel(this);
    debugLabel->setStyleSheet(
        "QLabel {"
        "  background-color: rgba(22, 19, 18, 230);"
        "  color: #a89888;"
        "  padding: 6px 10px;"
        "  border-radius: 6px;"
        "  border: 1px solid #4a3f37;"
        "  font-family: 'Cascadia Code', 'Consolas', monospace;"
        "  font-size: 11px;"
        "}"
    );

    if (videoRatio != QSize(0, 0)) {
        createVideoArea();
    }

    updateLayout();
}

void WorkspaceArea::createVideoArea() {
    if (videoArea) return;

    videoArea = new VideoArea(displayRatio, videoRatio, this);
    videoArea->setCursor(Qt::OpenHandCursor);
    videoArea->show();

    if (!pendingConfigCorners.isEmpty()) {
        QPoint tl = pendingConfigCorners[0];
        QPoint br = pendingConfigCorners[3];
        int w = br.x() - tl.x();
        int h = br.y() - tl.y();
        videoArea->realRect = QRect(tl.x(), tl.y(), w, h);
        pendingConfigCorners.clear();
    }

    if (noVideoLabel) {
        noVideoLabel->setVisible(false);
    }
}

// ── Config save: UNCHANGED ──
void WorkspaceArea::saveConfig(const QString& videoPath) {
    if (!videoArea) {
        qDebug() << "Cannot save: No video area loaded.";
        return;
    }

    videoArea->updateRealGeometry();
    QRect realR = videoArea->realRect;

    QJsonArray tl; tl.append(realR.x());                       tl.append(realR.y());
    QJsonArray tr; tr.append(realR.x() + realR.width());       tr.append(realR.y());
    QJsonArray bl; bl.append(realR.x());                       bl.append(realR.y() + realR.height());
    QJsonArray br; br.append(realR.x() + realR.width());       br.append(realR.y() + realR.height());
    QJsonObject config;
    config["videoPath"] = videoPath;
    QJsonArray cornerArray;
    cornerArray.append(tl);
    cornerArray.append(tr);
    cornerArray.append(bl);
    cornerArray.append(br);
    config["cornerLocation"] = cornerArray;

    QString configPath = getConfigPath();
    QDir configDir = QFileInfo(configPath).absoluteDir();
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }

    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(config);
        file.write(doc.toJson());
        qDebug() << "Config saved to" << configPath;
    }
}

// ═════════════════════════════════════════════
//  MainWindow Implementation
// ═════════════════════════════════════════════
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("HyprLarp Configuration Tools");
    resize(960, 680);
    setMinimumSize(600, 440);

    // ── Center window on screen ──
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        move((sg.width() - width()) / 2 + sg.x(),
             (sg.height() - height()) / 2 + sg.y());
    }

    monitorRatio = getMonitorResolution();
    videoRatio = QSize(0, 0);

    loadConfig();

    qDebug() << "Monitor:" << monitorRatio.width() << "x" << monitorRatio.height();
    if (videoRatio != QSize(0, 0)) {
        qDebug() << "Loaded Video:" << videoRatio.width() << "x" << videoRatio.height();
    }

    // ── Central widget ──
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("CentralWidget");
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Header Widget (Top Area) ──
    QWidget* headerWidget = new QWidget(centralWidget);
    headerWidget->setObjectName("HeaderWidget");
    QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(20, 15, 20, 10);
    headerLayout->setSpacing(4);

    QLabel* titleLabel = new QLabel("HyprLarp Configuration Tools", headerWidget);
    titleLabel->setObjectName("AppTitle");
    headerLayout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(
        "Drag the overlay to reposition · Drag corners to resize · Press Esc to reset",
        headerWidget);
    subtitleLabel->setObjectName("AppSubtitle");
    headerLayout->addWidget(subtitleLabel);

    QLabel* filePathLabel = new QLabel("No video file loaded", headerWidget);
    filePathLabel->setObjectName("FilePathLabel");
    headerLayout->addWidget(filePathLabel);

    mainLayout->addWidget(headerWidget);

    // ── Workspace Wrapper (Middle Area) ──
    QWidget* wsWrapper = new QWidget(centralWidget);
    wsWrapper->setObjectName("WsWrapper");
    QVBoxLayout* wsLayout = new QVBoxLayout(wsWrapper);
    wsLayout->setContentsMargins(20, 20, 20, 20);
    wsLayout->setSpacing(0);
    
    workspace = new WorkspaceArea(monitorRatio, videoRatio);
    wsLayout->addWidget(workspace);
    mainLayout->addWidget(wsWrapper, 1);

    if (!configCorners.isEmpty()) {
        workspace->pendingConfigCorners = configCorners;
    }

    // ── Footer Widget (Bottom Area) ──
    QWidget* footerWidget = new QWidget(centralWidget);
    footerWidget->setObjectName("FooterWidget");
    QHBoxLayout* controlsLayout = new QHBoxLayout(footerWidget);
    controlsLayout->setContentsMargins(20, 10, 20, 15);
    controlsLayout->setSpacing(10);

    openBtn = new QPushButton("Open Video");
    openBtn->setObjectName("PrimaryBtn");
    openBtn->setToolTip("Open a video file  (Ctrl+O)");
    QObject::connect(openBtn, &QPushButton::clicked, [this]() { this->openVideo(); });
    controlsLayout->addWidget(openBtn);

    saveBtn = new QPushButton("Save Config");
    saveBtn->setToolTip("Save corner positions to config  (Ctrl+S)");
    QObject::connect(saveBtn, &QPushButton::clicked, [this]() { this->saveConfig(); });
    controlsLayout->addWidget(saveBtn);

    controlsLayout->addStretch();

    resetBtn = new QPushButton("Reset");
    resetBtn->setObjectName("DangerBtn");
    resetBtn->setToolTip("Reset position and size  (Esc)");
    QObject::connect(resetBtn, &QPushButton::clicked, [this]() { this->workspace->resetVideo(); });
    controlsLayout->addWidget(resetBtn);

    QPushButton* exitBtn = new QPushButton("Exit");
    exitBtn->setToolTip("Close application  (Ctrl+Q)");
    QObject::connect(exitBtn, &QPushButton::clicked, this, &QMainWindow::close);
    controlsLayout->addWidget(exitBtn);

    mainLayout->addWidget(footerWidget);

    // ── Initialize workspace UI ──
    workspace->initUi();

    if (videoPath.isEmpty()) {
        workspace->noVideoLabel->setVisible(true);
    } else {
        filePathLabel->setText("File: " + QFileInfo(videoPath).fileName());
        statusBar()->showMessage(
            QString("Loaded: %1  (%2×%3)")
                .arg(QFileInfo(videoPath).fileName())
                .arg(videoRatio.width())
                .arg(videoRatio.height()),
            5000);
    }

    // ── Keyboard shortcuts ──
    new QShortcut(QKeySequence("Ctrl+O"), this, [this]() { this->openVideo(); });
    new QShortcut(QKeySequence("Ctrl+S"), this, [this]() { this->saveConfig(); });
    new QShortcut(QKeySequence("Escape"), this, [this]() { this->workspace->resetVideo(); });
    new QShortcut(QKeySequence("R"),      this, [this]() { this->workspace->resetVideo(); });
    new QShortcut(QKeySequence("Ctrl+Q"), this, [this]() { this->close(); });
}

// ── Config load: UNCHANGED ──
void MainWindow::loadConfig() {
    QString configPath = getConfigPath();
    QFile file(configPath);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                QJsonObject config = doc.object();

                QString vPath = config.value("videoPath").toString();
                QJsonArray cornersArray = config.value("cornerLocation").toArray();

                if (!vPath.isEmpty() && QFile::exists(vPath) && cornersArray.size() == 4) {
                    videoPath = vPath;
                    videoRatio = getVideoResolution(vPath);
                    configCorners.clear();
                    for (int i = 0; i < 4; i++) {
                        QJsonArray c = cornersArray.at(i).toArray();
                        configCorners.append(QPoint(c.at(0).toInt(), c.at(1).toInt()));
                    }
                    qDebug() << "Loaded config from" << configPath;
                } else {
                    qDebug() << "Config ignored: Video file missing or corners invalid.";
                }
            } else {
                qDebug() << "Error reading config: Invalid JSON";
            }
        }
    }
}

void MainWindow::openVideo() {
    QString filepath = QFileDialog::getOpenFileName(
        this, "Open Video File", "", "Video Files (*.mp4 *.avi *.mkv *.mov *.webm *.flv)");
    if (!filepath.isEmpty()) {
        videoPath = filepath;
        videoRatio = getVideoResolution(filepath);

        workspace->videoRatio = videoRatio;

        if (!workspace->videoArea) {
            workspace->createVideoArea();
        } else {
            workspace->videoArea->videoRatio = videoRatio;
            workspace->videoArea->realRect = QRect(0, 0, 0, 0);
        }

        workspace->updateLayout();
        workspace->videoArea->update();

        // Update File Path Label
        QLabel* lbl = centralWidget()->findChild<QLabel*>("FilePathLabel");
        if (lbl) {
            lbl->setText("File: " + QFileInfo(filepath).fileName());
        }

        statusBar()->showMessage(
            QString("Loaded: %1  (%2×%3)")
                .arg(QFileInfo(filepath).fileName())
                .arg(videoRatio.width())
                .arg(videoRatio.height()),
            4000);
    }
}

void MainWindow::saveConfig() {
    if (videoPath.isEmpty() || !workspace->videoArea) {
        statusBar()->showMessage("Nothing to save — no video loaded.", 3000);
        qDebug() << "No video loaded to save.";
        return;
    }
    workspace->saveConfig(videoPath);
    statusBar()->showMessage(
        QString("Config saved → %1").arg(getConfigPath()), 4000);
}

// ── Main setup ──
int setup(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Global font
    QFont appFont("Segoe UI", 10);
    app.setFont(appFont);

    // Global stylesheet
    app.setStyleSheet(kAppQSS);

    MainWindow window;
    window.showFullScreen();
    window.show();
    return app.exec();
}