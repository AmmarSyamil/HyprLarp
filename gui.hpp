#pragma once
#ifndef VIDEOLAYOUTTOOL_HPP
#define VIDEOLAYOUTTOOL_HPP

#include <QWidget>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QRect>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QVector>
#include <QColor>

// Forward declarations
class WorkspaceArea;
class VideoArea;

// Utility functions (declarations)
QSize getMonitorResolution();
QSize getVideoResolution(const QString& path);
QString getConfigPath();

class VideoArea : public QWidget
{
    Q_OBJECT

public:
    VideoArea(QSize monitorRatio, QSize videoRatio, QWidget* parent = nullptr);

    void updateRealGeometry();
    QRect getCornerRect(const QString& name) const;
    QPoint getCornerCenter(const QString& name) const;
    QString checkCornerHover(const QPoint& pos) const;
    void performResize(const QPoint& currentPos);
    void resetGeometry(const QRect& rect);

    // Public members (exposed for WorkspaceArea)
    QSize monitorRatio;
    QSize videoRatio;
    QColor bgColor;
    QColor borderColor;
    QColor handleColor;
    int handleRadius;
    int handleRadiusHover;
    bool dragging;
    bool resizing;
    QString activeCorner;
    QPoint startPos;
    QRect widgetStartGeo;
    double currentScale;
    QRect realRect;
    QString hoveredCorner;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
};

class WorkspaceArea : public QWidget
{
    Q_OBJECT

public:
    WorkspaceArea(QSize displayRatio, QSize videoRatio, QWidget* parent = nullptr);

    void updateLayout();
    void updateVideoMask();
    void resetVideo();
    void updateDebugInfo();
    void initUi();
    void createVideoArea();
    void saveConfig(const QString& videoPath);

    // Public members
    QSize displayRatio;
    QSize videoRatio;
    VideoArea* videoArea;
    QLabel* debugLabel;
    QLabel* noVideoLabel;
    QRect currentWorkspaceRect;
    QSize videoBaseSize;
    QVector<QPoint> pendingConfigCorners;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);

    void loadConfig();
    void openVideo();
    void saveConfig();

    // Public members
    QSize monitorRatio;
    QString videoPath;
    QSize videoRatio;
    QVector<QPoint> configCorners;
    WorkspaceArea* workspace;
    QPushButton* openBtn;
    QPushButton* saveBtn;
    QPushButton* resetBtn;
};

int setup(int argc, char* argv[]);

#endif // VIDEOLAYOUTTOOL_HPP