#include "DropFilesDialog.h"
#include "Utils.h"

#include <QDirIterator>
#include <QFileInfo>

DropFilesDialog::DropFilesDialog(const QList<QUrl>& urls, QWidget* parent) : QDialog(parent), urls_(urls)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui_.setupUi(this);

	QObject::connect(ui_.cancelButton, &QPushButton::clicked, this, &QDialog::close);

	f_ = QtConcurrent::run(this, &DropFilesDialog::proc);
}

DropFilesDialog::~DropFilesDialog()
{
}

QList<QString>& DropFilesDialog::list()
{
	return list_;
}

void DropFilesDialog::closeEvent(QCloseEvent* e)
{
	quit_.store(1);
	f_.waitForFinished();

	QDialog::closeEvent(e);
}

void DropFilesDialog::updateProgress(int count)
{
	ui_.infoLabel->setText(QString(u8"已发现 %1 个图片").arg(count));
}

void DropFilesDialog::proc()
{
	int count = 0;

	QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection, Q_ARG(int, count));

	for (const QUrl& url : urls_)
	{
		if (quit_.load()) {
			return;
		}

		const QString& path = url.toLocalFile();

		QFileInfo info(path);

		if (isImageFile(info))
		{
			list_.append(path);
			QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection, Q_ARG(int, ++count));
		}
		else if (info.isDir())
		{
			QDirIterator it(path, QDirIterator::Subdirectories);

			while (it.hasNext())
			{
				if (quit_.load()) {
					return;
				}

				const QString& path = it.next();

				if (isImageFile(path))
				{
					list_.append(path);
					QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection, Q_ARG(int, ++count));
				}
			}
		}
	}

	QMetaObject::invokeMethod(this, "done", Qt::QueuedConnection, Q_ARG(int, 1));
}

