//-----------------------------------------------------------------------------
/** @file pentobi/Main.cpp
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

#include <QApplication>
#include <QTranslator>
#include <QQuickStyle>
#include <QtQml>
#include <QtWebView>
#include "AnalyzeGameModel.h"
#include "AndroidUtils.h"
#include "GameModel.h"
#include "ImageProvider.h"
#include "PlayerModel.h"
#include "RatingModel.h"
#include "SyncSettings.h"
#include "libboardgame_util/Log.h"

#ifndef Q_OS_ANDROID
#include <QCommandLineParser>
#endif

#ifdef Q_OS_LINUX
#include <QQuickWindow>
#endif

//-----------------------------------------------------------------------------

namespace {

#ifdef Q_OS_ANDROID

int mainAndroid()
{
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("pentobi"), new ImageProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("initialFile"),
                                             QStringLiteral(""));
    engine.rootContext()->setContextProperty(QStringLiteral("isDesktop"),
                                             QVariant(false));
#ifdef QT_DEBUG
    engine.rootContext()->setContextProperty(QStringLiteral("isDebug"),
                                             QVariant(true));
#else
    engine.rootContext()->setContextProperty(QStringLiteral("isDebug"),
                                             QVariant(false));
#endif
    engine.load(QUrl(QStringLiteral("qrc:///qml/Main.qml")));
    if (engine.rootObjects().empty())
        return 1;
    return QGuiApplication::exec();
}

#else // ! defined(Q_OS_ANDROID)

int mainDesktop()
{
    QIcon icon;
    icon.addFile(QStringLiteral(":/pentobi_icon/pentobi.svg"));
    icon.addFile(QStringLiteral(":/pentobi_icon/pentobi-16.svg"));
    icon.addFile(QStringLiteral(":/pentobi_icon/pentobi-32.svg"));
    icon.addFile(QStringLiteral(":/pentobi_icon/pentobi-64.svg"));
    QGuiApplication::setWindowIcon(icon);
    QGuiApplication::setDesktopFileName(
                QStringLiteral("io.sourceforge.pentobi"));
    QCommandLineParser parser;
    auto maxSupportedLevel = Player::max_supported_level;
    QCommandLineOption optionMaxLevel(
                QStringLiteral("maxlevel"),
                QStringLiteral("Set maximum level to <n>."),
                QStringLiteral("n"),
                QString::number(PlayerModel::maxLevel));
    parser.addOption(optionMaxLevel);
    QCommandLineOption optionNoBook(
                QStringLiteral("nobook"),
                QStringLiteral("Do not use opening books."));
    QCommandLineOption optionMobile(
                QStringLiteral("mobile"),
                QStringLiteral("Use layout optimized for smartphones."));
    parser.addOption(optionMobile);
    parser.addOption(optionNoBook);
    QCommandLineOption optionNoDelay(
                QStringLiteral("nodelay"),
                QStringLiteral("Do not delay fast computer moves."));
    parser.addOption(optionNoDelay);
    QCommandLineOption optionSeed(
                QStringLiteral("seed"),
                QStringLiteral("Set random seed to <n>."),
                QStringLiteral("n"));
    parser.addOption(optionSeed);
    QCommandLineOption optionThreads(
                QStringLiteral("threads"),
                QStringLiteral("Use <n> threads (0=auto)."),
                QStringLiteral("n"));
    parser.addOption(optionThreads);
#ifndef LIBBOARDGAME_DISABLE_LOG
    QCommandLineOption optionVerbose(
                QStringLiteral("verbose"),
                QStringLiteral("Print logging information to standard error."));
    parser.addOption(optionVerbose);
#endif
    parser.addPositionalArgument(
                QStringLiteral("file.blksgf"),
                QStringLiteral("Blokus SGF file to open (optional)."));
    parser.addHelpOption();
    parser.process(*QCoreApplication::instance());
    try
    {
#ifndef LIBBOARDGAME_DISABLE_LOG
        if (! parser.isSet(optionVerbose))
            libboardgame_util::disable_logging();
#endif
        if (parser.isSet(optionNoBook))
            PlayerModel::noBook = true;
        if (parser.isSet(optionNoDelay))
            PlayerModel::noDelay = true;
        bool ok;
        auto maxLevel = parser.value(optionMaxLevel).toUInt(&ok);
        if (! ok || maxLevel < 1 || maxLevel > maxSupportedLevel)
            throw runtime_error("--maxlevel must be between 1 and "
                                + libboardgame_util::to_string(maxSupportedLevel));
        PlayerModel::maxLevel = maxLevel;
        if (parser.isSet(optionSeed))
        {
            auto seed = parser.value(optionSeed).toUInt(&ok);
            if (! ok)
                throw runtime_error("--seed must be a positive number");
            libboardgame_util::RandomGenerator::set_global_seed(seed);
        }
        if (parser.isSet(optionThreads))
        {
            auto nuThreads = parser.value(optionThreads).toUInt(&ok);
            if (! ok)
                throw runtime_error("--threads must be a positive number");
            PlayerModel::nuThreads = nuThreads;
        }
        bool isDesktop = ! parser.isSet(optionMobile);
        QString initialFile;
        auto args = parser.positionalArguments();
        if (args.size() > 1)
            throw runtime_error("Too many arguments");
        if (! args.empty())
            initialFile = args.at(0);
        if (QQuickStyle::name().isEmpty() && isDesktop)
            QQuickStyle::setStyle(QStringLiteral("Fusion"));
        QQmlApplicationEngine engine;
        engine.addImageProvider(QStringLiteral("pentobi"), new ImageProvider);
        engine.rootContext()->setContextProperty(QStringLiteral("initialFile"),
                                                 initialFile);
        engine.rootContext()->setContextProperty(QStringLiteral("isDesktop"),
                                                 isDesktop);
#ifdef QT_DEBUG
        engine.rootContext()->setContextProperty(QStringLiteral("isDebug"),
                                                 QVariant(true));
#else
        engine.rootContext()->setContextProperty(QStringLiteral("isDebug"),
                                                 QVariant(false));
#endif
        engine.load(QUrl(QStringLiteral("qrc:///qml/Main.qml")));
        if (engine.rootObjects().empty())
            return 1;
        return QGuiApplication::exec();
    }
    catch (const exception& e)
    {
        LIBBOARDGAME_LOG("Error: ", e.what());
        return 1;
    }
}

#endif // Q_OS_ANDROID

} // namespace

//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    libboardgame_util::LogInitializer log_initializer;
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setOrganizationName(QStringLiteral("Pentobi"));
    QCoreApplication::setApplicationName(QStringLiteral("Pentobi"));
#ifdef VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(VERSION));
#endif
    QGuiApplication app(argc, argv);
    QtWebView::initialize();
    qmlRegisterType<AnalyzeGameModel>("pentobi", 1, 0, "AnalyzeGameModel");
    qmlRegisterType<AndroidUtils>("pentobi", 1, 0, "AndroidUtils");
    qmlRegisterType<GameModel>("pentobi", 1, 0, "GameModel");
    qmlRegisterType<PlayerModel>("pentobi", 1, 0, "PlayerModel");
    qmlRegisterType<RatingModel>("pentobi", 1, 0, "RatingModel");
    qmlRegisterType<SyncSettings>("pentobi", 1, 0, "SyncSettings");
    qmlRegisterInterface<AnalyzeGameElement>("AnalyzeGameElement");
    qmlRegisterInterface<GameMove>("GameModelMove");
    qmlRegisterInterface<PieceModel>("PieceModel");
    QString locale = QLocale::system().name();
    QTranslator translator;
    translator.load("qml_" + locale, QStringLiteral(":qml/i18n"));
    QCoreApplication::installTranslator(&translator);
#ifdef Q_OS_ANDROID
    return mainAndroid();
#else
    return mainDesktop();
#endif
}

//-----------------------------------------------------------------------------
