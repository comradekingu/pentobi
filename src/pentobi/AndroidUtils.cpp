//-----------------------------------------------------------------------------
/** @file pentobi/AndroidUtils.cpp
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

#include "AndroidUtils.h"

#include <QCoreApplication>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QDir>
#include <QDirIterator>
#include <QtAndroidExtras/QtAndroid>
#include <QtAndroidExtras/QAndroidJniObject>
#endif

//-----------------------------------------------------------------------------

bool AndroidUtils::checkPermission(const QString& permission)
{
#ifdef Q_OS_ANDROID
    return QtAndroid::checkPermission(permission) ==
           QtAndroid::PermissionResult::Granted;
#else
    Q_UNUSED(permission);
    return true;
#endif
}

QUrl AndroidUtils::extractHelp(const QString& language)
{
#ifdef Q_OS_ANDROID
    if (language != QStringLiteral("C"))
        // Other languages use pictures from C
        extractHelp(QStringLiteral("C"));
    auto activity = QtAndroid::androidActivity();
    auto filesDir =
            activity.callObjectMethod("getFilesDir", "()Ljava/io/File;");
    if (! filesDir.isValid())
        return {};
    auto filesDirString = filesDir.callObjectMethod("toString",
                                                    "()Ljava/lang/String;");
    if (! filesDirString.isValid())
        return {};
    QDir dir(filesDirString.toString() + "/help/"
             + QCoreApplication::applicationVersion() + "/" + language
             + "/pentobi");
    auto dirPath = dir.path();
    if (QFileInfo::exists(dirPath + "/index.html"))
        return QUrl::fromLocalFile(dirPath + "/index.html");
    if (! QFileInfo::exists(filesDirString.toString() + "/help/"
                            + QCoreApplication::applicationVersion()
                            + "/C/pentobi/index.html"))
        // No need to keep files from older versions around
        QDir(filesDirString.toString() + "/help").removeRecursively();
    QDirIterator it(":qml/help/" + language + "/pentobi");
    while (it.hasNext())
    {
        it.next();
        if (! it.fileInfo().isFile())
            continue;
        QFile dest(dirPath + "/" + it.fileName());
        QFileInfo(dest).dir().mkpath(QStringLiteral("."));
        dest.remove();
        QFile::copy(it.filePath(), dest.fileName());
    }
    auto file = QFileInfo(dirPath + "/index.html").absoluteFilePath();
    return QUrl::fromLocalFile(file);
#else
    Q_UNUSED(language);
    return {};
#endif
}

QUrl AndroidUtils::getDefaultFolder()
{
#ifdef Q_OS_ANDROID
    QUrl fallback(QStringLiteral("file:///sdcard"));
    auto file = QAndroidJniObject::callStaticObjectMethod(
                "android/os/Environment", "getExternalStorageDirectory",
                "()Ljava/io/File;");
    if (! file.isValid())
        return fallback;
    auto fileString = file.callObjectMethod("toString",
                                            "()Ljava/lang/String;");
    if (! fileString.isValid())
        return fallback;
    return QUrl::fromLocalFile(fileString.toString());
#else
    return QUrl::fromLocalFile(
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
#endif
}

void AndroidUtils::scanFile(const QString& pathname)
{
#ifdef Q_OS_ANDROID
    // Corresponding Java code:
    //   sendBroadcast(new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE,
    //                       Uri.fromFile(File(pathname).getCanonicalFile())));
    auto action = QAndroidJniObject::getStaticObjectField<jstring>(
                "android/content/Intent", "ACTION_MEDIA_SCANNER_SCAN_FILE");
    if (! action.isValid())
        return;
    auto pathnameString = QAndroidJniObject::fromString(pathname);
    QAndroidJniObject file("java/io/File", "(Ljava/lang/String;)V",
                           pathnameString.object<jstring>());
    if (! file.isValid())
        return;
    auto absoluteFile = file.callObjectMethod(
                "getAbsoluteFile", "()Ljava/io/File;");
    if (! absoluteFile.isValid())
        return;
    auto uri = QAndroidJniObject::callStaticObjectMethod(
                "android/net/Uri", "fromFile",
                "(Ljava/io/File;)Landroid/net/Uri;", absoluteFile.object());
    if (! uri.isValid())
        return;
    QAndroidJniObject intent("android/content/Intent",
                             "(Ljava/lang/String;Landroid/net/Uri;)V",
                             action.object<jstring>(), uri.object());
    if (! intent.isValid())
        return;
    QtAndroid::androidActivity().callMethod<void>(
                "sendBroadcast", "(Landroid/content/Intent;)V",
                intent.object());
#else
    Q_UNUSED(pathname);
#endif
}

//-----------------------------------------------------------------------------
