#include "JoinImageDialog.h"
#include "ErrorCode.h"

#include <QRegExpValidator>
#include <QLineEdit>
#include <QPainter>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QMessageBox>
#include <QFileDialog>
#include <QRegExp>

#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <assert.h>
#include <stdio.h>

namespace fs = boost::filesystem;

JoinImageDialog::JoinImageDialog(QList<QString>&& list, QWidget* parent)
	: QDialog(parent), list_(std::move(list)), relativePaths_(removeSameBasePaths(list_))
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui_.setupUi(this);

	ui_.progressBar->setVisible(false);

	QRegExp exp(R"([1-9][0-9]*)");

	ui_.widthBox->setValidator(new QRegExpValidator(exp));
	ui_.joinHeightBox->setValidator(new QRegExpValidator(exp));

	QObject::connect(ui_.cancelButton, &QPushButton::clicked, this, &QDialog::close);
	QObject::connect(ui_.startButton, &QPushButton::clicked, this, &JoinImageDialog::start);
	QObject::connect(ui_.browseButton, &QPushButton::clicked, this, &JoinImageDialog::browse);

	QFile loadFile("JoinImageConfig");

	if (loadFile.open(QIODevice::ReadOnly))
	{
		QJsonDocument loadDoc = QJsonDocument::fromJson(loadFile.readAll());

		QJsonObject configObject = loadDoc.object();

		ui_.outDirEdit->setText(configObject["outdir"].toString());
		ui_.widthBox->lineEdit()->setText(QString::number(configObject["width"].toInt()));
		ui_.joinHeightBox->lineEdit()->setText(QString::number(configObject["joinHeight"].toInt()));
		ui_.jpegQualityBox->setValue(configObject["jpegQuality"].toInt());
	}

	cancel_.store(0);
}

JoinImageDialog::~JoinImageDialog()
{
}

void JoinImageDialog::closeEvent(QCloseEvent* e)
{
	ui_.cancelButton->setEnabled(false);

	cancel_.store(1);
	f_.waitForFinished();

	QDialog::closeEvent(e);
}

void JoinImageDialog::browse()
{
	ui_.outDirEdit->setText(QFileDialog::getExistingDirectory(this));
}

void JoinImageDialog::start()
{
	QFileInfo outDirInfo(ui_.outDirEdit->text());

	if (!outDirInfo.exists() || !outDirInfo.isDir()) {
		ui_.outDirEdit->selectAll();
		ui_.outDirEdit->setFocus();
		return;
	}

	settings_.outputDir = ui_.outDirEdit->text();

	bool ok;

	settings_.width = QVariant(ui_.widthBox->currentText()).toInt(&ok);

	if (!ok || settings_.width < 10 || settings_.width > 4000) {
		ui_.widthBox->lineEdit()->selectAll();
		ui_.widthBox->setFocus();
		return;
	}

	settings_.joinHeight = QVariant(ui_.joinHeightBox->currentText()).toInt(&ok);

	if (!ok || settings_.joinHeight < 10 || settings_.joinHeight > 65535) {
		ui_.joinHeightBox->lineEdit()->selectAll();
		ui_.joinHeightBox->setFocus();
		return;
	}

	settings_.jpegQuality = ui_.jpegQualityBox->value();

	QFile saveFile("JoinImageConfig");

	if (saveFile.open(QIODevice::WriteOnly))
	{
		QJsonObject configObject;

		configObject["outdir"] = ui_.outDirEdit->text();
		configObject["width"] = settings_.width;
		configObject["joinHeight"] = settings_.joinHeight;
		configObject["jpegQuality"] = settings_.jpegQuality;

		QJsonDocument saveDoc(configObject);

		saveFile.write(saveDoc.toJson());
	}

	ui_.settingGroup->setEnabled(false);

	ui_.startButton->setEnabled(false);

	ui_.progressBar->setVisible(true);

	f_ = QtConcurrent::run(this, &JoinImageDialog::proc);
}

void JoinImageDialog::updateProgress(int v)
{
	ui_.progressBar->setValue(v);
}

void JoinImageDialog::handleError(int ec)
{
	if (ec == ec_cancel) {
		QMessageBox::information(this, "", u8"操作已取消");
	}
	else if (ec == ec_load_error) {
		QMessageBox::warning(this, "", u8"读文件时遇到错误");
	}
	else if (ec == ec_save_error) {
		QMessageBox::warning(this, "", u8"写文件时遇到错误");
	}

	done(0);
}

void JoinImageDialog::proc()
{
	fs::path outputPath = settings_.outputDir.toStdWString();

	outputPath /= boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time());

	int lastIndex = -1;
	fs::path lastBranch;

	for (int i = 0; i < list_.count(); ++i)
	{
		fs::path branch = fs::path(list_[i].toStdWString()).branch_path();

		if (i == 0) {
			lastIndex = 0;
			lastBranch = branch;
		}

		if (!boost::iequals(branch.generic_wstring(), lastBranch.generic_wstring()))
		{
			int ec = joinImages(QString::fromStdWString(outputPath.generic_wstring()), lastIndex, i);

			if (ec != ec_success)
			{
				QMetaObject::invokeMethod(this, "handleError", Qt::QueuedConnection, Q_ARG(int, ec));
				return;
			}

			QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection, Q_ARG(int, (double)i / (double)list_.count() * 100.0));

			lastIndex = i;
			lastBranch = branch;
		}

		if (i == list_.count() - 1)
		{
			int ec = joinImages(QString::fromStdWString(outputPath.generic_wstring()), lastIndex, list_.count());

			if (ec != ec_success)
			{
				QMetaObject::invokeMethod(this, "handleError", Qt::QueuedConnection, Q_ARG(int, ec));
				return;
			}

			QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection, Q_ARG(int, 100));
		}
	}

	QMetaObject::invokeMethod(this, "done", Qt::QueuedConnection, Q_ARG(int, 1));
}

int JoinImageDialog::joinImages(const QString& outputPath, int first, int last)
{
	QList<QImage> images;

	int ec = loadImages(images, first, last);

	if (ec != ec_success) {
		return ec;
	}

	QImage joiningBoard(settings_.width, settings_.joinHeight, QImage::Format_RGB32);

	int imageIndex = 0;

	QString fullOutputPath = makeOutputPath(outputPath, relativePaths_[first]);
	int number = 1;

	for (;;)
	{
		if (cancel_.load()) {
			return ec_cancel;
		}

		int h = feedCuttingBoard(joiningBoard, 0, images, imageIndex);

		if (h == 0) {
			break;
		}

		if (h == settings_.joinHeight)
		{
			if (!joiningBoard.save(generateFilename(fullOutputPath, number++), "JPG", settings_.jpegQuality)) {
				return ec_save_error;
			}
		}
		else
		{
			QImage image = joiningBoard.copy(0, 0, settings_.width, h);

			if (!image.save(generateFilename(fullOutputPath, number++), "JPG", settings_.jpegQuality)) {
				return ec_save_error;
			}
		}
	}

	return ec_success;
}

int JoinImageDialog::feedCuttingBoard(QImage& board, int boardY, QList<QImage>& images, int& imageIndex)
{
	int result = 0;

	int boardSpace = board.height() - boardY;

	while (imageIndex < images.count())
	{
		QImage& image = images[imageIndex];

		if (image.height() > boardSpace) {
			break;
		}

		QPainter painter(&board);
		painter.drawImage(0, boardY, image);
		painter.end();

		boardY += image.height();
		boardSpace -= image.height();
		result += image.height();

		++imageIndex;
	}

	return result;
}

int JoinImageDialog::loadImages(QList<QImage>& images, int first, int last)
{
	for (int i = first; i < last; ++i)
	{
		if (cancel_.load()) {
			return ec_cancel;
		}

		QImage image;

		if (!image.load(list_[i])) {
			return ec_load_error;
		}

		if (image.format() != QImage::Format_RGB32) {
			image = image.convertToFormat(QImage::Format_RGB32);
		}

		if (image.width() != settings_.width) {
			image = image.scaledToWidth(settings_.width, Qt::SmoothTransformation);
		}

		images.append(image);
	}

	return ec_success;
}

QString JoinImageDialog::generateFilename(const QString& base, int number)
{
	fs::path p = base.toStdWString();

	char buf[32];

	sprintf(buf, "%04d.jpg", number);

	p = p / buf;

	return QString::fromStdWString(p.generic_wstring());
}

QString JoinImageDialog::makeOutputPath(const QString& basePath, const QString& relativePath)
{
	fs::path p(basePath.toStdWString());

	if (!relativePath.isEmpty())
	{
		QStringList parts = relativePath.split(QRegExp("[\\/]"));

		for (QString& part : parts)
		{
			part.replace(QRegExp("[\\/:*?\"<>|]"), "_");
			p /= part.toStdWString();
		}
	}

	if (!fs::exists(p)) {
		fs::create_directories(p);
	}

	return QString::fromStdWString(p.generic_wstring());
}

QList<QString> JoinImageDialog::removeSameBasePaths(const QList<QString>& list)
{
	QList<QStringList> v;

	for (int i = 0; i < list.count(); ++i)
	{
		QString branchPath = QString::fromStdWString(fs::path(list[i].toStdWString()).branch_path().wstring());
		v.append(branchPath.split(QRegExp("[\\/]")));
	}

	int n = 0;

	for (;;)
	{
		QString part;

		for (int i = 0; i < v.count(); ++i)
		{
			if (n >= v[i].count()) {
				goto quit;
			}

			if (i == 0) {
				part = v[0][n];
				continue;
			}

			if (v[i][n] != part) {
				goto quit;
			}
		}

		++n;
	}

quit:
	if (n > 0)
	{
		for (int i = 0; i < v.count(); ++i) {
			for (int j = 0; j < n; ++j) {
				v[i].removeFirst();
			}
		}
	}


	QList<QString> result;

	for (int i = 0; i < v.count(); ++i)
	{
		QString path;

		for (int j = 0; j < v[i].count(); ++j)
		{
			if (j > 0) {
				path.append("\\");
			}

			path.append(v[i][j]);
		}

		result.append(path);
	}

	return result;
}

