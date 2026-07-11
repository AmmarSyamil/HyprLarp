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

#include <opencv2/opencv.hpp>
#include <cmath>
#include <algorithm>

// Utility functions
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

// VideoArea Implementation
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
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QBrush(bgColor));
    painter.setPen(QPen(borderColor, 2));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    QStringList corners = {"TL", "TR", "BL", "BR"};
    for (const QString& name : corners) {
        int radius = handleRadius;
        if (hoveredCorner == name) {
            radius = handleRadiusHover;
        }
        QPoint center = getCornerCenter(name);
        painter.setBrush(QBrush(handleColor));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(center, radius, radius);
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

// WorkspaceArea Implementation
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
}

void WorkspaceArea::resizeEvent(QResizeEvent*) {
    updateLayout();
}

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
        QString msg = QString("Monitor: %1x%2\nVideo: %3x%4\nScale: x%5")
            .arg(displayRatio.width()).arg(displayRatio.height())
            .arg(vw).arg(vh)
            .arg(scale, 0, 'f', 2);
        debugLabel->setText(msg);
        debugLabel->adjustSize();
        debugLabel->move(5, height() - debugLabel->height() - 5);
    }
}

void WorkspaceArea::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(currentWorkspaceRect, QBrush(QColor(0, 255, 0, 20)));
    painter.setPen(QPen(QColor(0, 255, 0), 2));
    painter.drawRect(currentWorkspaceRect);
}

void WorkspaceArea::initUi() {
    noVideoLabel = new QLabel("No Video Loaded\n\nClick 'Open Video' to select a file", this);
    noVideoLabel->setAlignment(Qt::AlignCenter);
    noVideoLabel->setStyleSheet(
        "QLabel { color: #ffffff; font-size: 18px; font-weight: bold; "
        "background-color: rgba(0, 0, 0, 120); border-radius: 10px; padding: 20px; }"
    );
    noVideoLabel->setVisible(false);

    debugLabel = new QLabel(this);
    debugLabel->setStyleSheet("background-color: rgba(0,0,0,180); color: white; padding: 2px;");

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

// MainWindow Implementation
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Video Layout Tool (Resizable)");
    resize(800, 600);

    monitorRatio = getMonitorResolution();
    videoRatio = QSize(0, 0);

    loadConfig();

    qDebug() << "Monitor:" << monitorRatio.width() << "x" << monitorRatio.height();
    if (videoRatio != QSize(0, 0)) {
        qDebug() << "Loaded Video:" << videoRatio.width() << "x" << videoRatio.height();
    }

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(10);

    workspace = new WorkspaceArea(monitorRatio, videoRatio);
    mainLayout->addWidget(workspace);

    if (!configCorners.isEmpty()) {
        workspace->pendingConfigCorners = configCorners;
    }

    QHBoxLayout* controlsLayout = new QHBoxLayout();

    openBtn = new QPushButton("Open Video");
    QObject::connect(openBtn, &QPushButton::clicked, [this]() { this->openVideo(); });
    controlsLayout->addWidget(openBtn);

    saveBtn = new QPushButton("Save Config");
    QObject::connect(saveBtn, &QPushButton::clicked, [this]() { this->saveConfig(); });
    controlsLayout->addWidget(saveBtn);

    resetBtn = new QPushButton("Reset Position & Size");
    QObject::connect(resetBtn, &QPushButton::clicked, [this]() { this->workspace->resetVideo(); });
    controlsLayout->addStretch();
    controlsLayout->addWidget(resetBtn);

    mainLayout->addLayout(controlsLayout);

    workspace->initUi();

    if (videoPath.isEmpty()) {
        workspace->noVideoLabel->setVisible(true);
    }
}

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
        this, "Open Video File", "", "Video Files (*.mp4 *.avi *.mkv *.mov)");
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
    }
}

void MainWindow::saveConfig() {
    if (videoPath.isEmpty() || !workspace->videoArea) {
        qDebug() << "No video loaded to save.";
        return;
    }
    workspace->saveConfig(videoPath);
}

// Main setup
int setup(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}