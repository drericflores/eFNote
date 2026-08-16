#include "MainWindow.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("eFNote"));
    QApplication::setApplicationDisplayName(QStringLiteral("eFNote"));
    QApplication::setApplicationVersion(QStringLiteral(EFNOTE_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Dr. Eric O. Flores"));
    QApplication::setOrganizationDomain(QStringLiteral("github.com/drericflores"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/efnote.svg")));

    QStringList files;
    for (int index = 1; index < argc; ++index) {
        const QFileInfo candidate(QString::fromLocal8Bit(argv[index]));
        if (candidate.isFile()) {
            files.append(candidate.absoluteFilePath());
        }
    }

    MainWindow window(files);
    window.show();
    return app.exec();
}
