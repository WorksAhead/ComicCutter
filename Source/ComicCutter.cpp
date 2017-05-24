#include "ComicCutter.h"
#include "DropFilesDialog.h"
#include "CutImageDialog.h"
#include "JoinImageDialog.h"
#include "Utils.h"

#include <QPixmap>
#include <QImage>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QList>
#include <QUrl>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>

#include <QDebug>

ComicCutter::ComicCutter(QWidget *parent) : QMainWindow(parent)
{
	ui_.setupUi(this);

	setWindowTitle("ComicCutter 1.3a");

	QObject::connect(ui_.actionExit, &QAction::triggered, this, &QMainWindow::close);

	setAcceptDrops(true);

	QDir dataDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

	if (!dataDir.exists())
	{
		dataDir.mkpath(".");
	}
}

void ComicCutter::onDropFiles(int result)
{
	if (result == 0) {
		return;
	}

	DropFilesDialog* sender = qobject_cast<DropFilesDialog*>(QObject::sender());

	if (sender == 0) {
		return;
	}

	if (sender->list().count() == 0) {
		return;
	}

	QMessageBox msgBox(this);

	msgBox.setText(u8"下一步做什么");

	QPushButton* cutButton = msgBox.addButton(u8"切分", QMessageBox::AcceptRole);
	QPushButton* joinButton = msgBox.addButton(u8"合并", QMessageBox::AcceptRole);

	msgBox.addButton(u8"取消", QMessageBox::RejectRole);

	msgBox.exec();

	if (msgBox.clickedButton() == cutButton)
	{
		CutImageDialog* d = new CutImageDialog(std::move(sender->list()), this);

		d->setAttribute(Qt::WA_DeleteOnClose);

		d->show();
	}
	else if (msgBox.clickedButton() == joinButton)
	{
		JoinImageDialog* d = new JoinImageDialog(std::move(sender->list()), this);

		d->setAttribute(Qt::WA_DeleteOnClose);

		d->show();
	}
}

void ComicCutter::dragEnterEvent(QDragEnterEvent* e)
{
	enum {
		dir_mask = 1 << 0,
		image_mask = 1 << 1,
	};

	if (e->mimeData()->hasUrls())
	{
		QList<QUrl> urls = e->mimeData()->urls();

		int mask = 0;

		for (const QUrl& url : urls)
		{
			QFileInfo info(url.toLocalFile());

			if (info.isDir()) {
				mask = mask | dir_mask;
			}
			else if (isImageFile(info))
			{
				mask = mask | image_mask;
			}
			else {
				return;
			}
		}

		if ((mask & dir_mask) && (mask & image_mask)) {
			return;
		}

		e->acceptProposedAction();
	}
}

void ComicCutter::dropEvent(QDropEvent* e)
{
	DropFilesDialog* d = new DropFilesDialog(e->mimeData()->urls(), this);

	d->setAttribute(Qt::WA_DeleteOnClose);

	QObject::connect(d, &QDialog::finished, this, &ComicCutter::onDropFiles);

	d->show();
}

