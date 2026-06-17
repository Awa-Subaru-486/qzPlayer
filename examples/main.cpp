#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStandardPaths>

#ifdef USE_QWINKIT
#include <QWKQuick/qwkquickglobal.h>
#endif

#ifdef  Q_OS_WINDOWS
#include <windows.h>
#endif

import qml_register_types_qzPlayer;
import qml_register_types_qzTheme;
import qml_register_types_qzMultimedia;
import qzLog;

static QtMessageHandler originalHandler = nullptr;

static void qzMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const auto formatted = qFormatLogMessage(type, context, msg).toStdString();
    switch (type) {
    case QtDebugMsg:    qz::Log::debug("{}", formatted);    break;
    case QtInfoMsg:     qz::Log::info("{}", formatted);     break;
    case QtWarningMsg:  qz::Log::warn("{}", formatted);     break;
    case QtCriticalMsg: qz::Log::error("{}", formatted);    break;
    case QtFatalMsg:    qz::Log::critical("{}", formatted); break;
    }
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WINDOWS
    SetConsoleOutputCP(CP_UTF8);
#endif

    const QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("qzPlayer");
    QGuiApplication::setApplicationName("qzPlayerExample");

    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
#ifndef Q_OS_ANDROID
     QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath() + "/plugins");
#endif
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

#ifdef Q_OS_ANDROID
    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/logs";
#else
    const QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/logs";
#endif

    // qz::Log::set_filter_rules("qz.multimedia.ffmpeg.codecstorage=true;qz.multimedia.playbackengine.codec=true;qz.multimedia.ffmpeg.hwaccel=true");
    const auto app_name = QGuiApplication::applicationName();
    qz::Log::init(app_name.toStdString(), logDir.toStdString(), 1, 2);
    originalHandler = qInstallMessageHandler(qzMessageHandler);

    QQmlApplicationEngine engine;

#ifdef USE_QWINKIT
    QWK::registerTypes(&engine);
#endif
    qz::qml_register_types_qz_multimedia(&engine);
    qz::qml_register_types_qz_player(&engine);
    qz::qml_register_types_qz_theme(&engine);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
    [] {
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [] {
        qz::Log::flush();
        qz::Log::shutdown();
    });

    engine.loadFromModule(QStringLiteral("qz.example"),
#ifdef Q_OS_ANDROID
        QStringLiteral("AndroidMain")
#else
        QStringLiteral("WindowsMain")
#endif
    );

    const int ret = QGuiApplication::exec();
    qz::Log::shutdown();
    return ret;
}
