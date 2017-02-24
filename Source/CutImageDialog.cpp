#include "CutImageDialog.h"
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
#include <QDesktopServices>

#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <assert.h>
#include <stdio.h>

namespace fs = boost::filesystem;

CutImageDialog::CutImageDialog(QList<QString>&& list, QWidget* parent)
	: QDialog(parent), list_(std::move(list)), relativePaths_(removeSameBasePaths(list_))
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui_.setupUi(this);

	ui_.progressBar->setVisible(false);

	QRegExp exp(R"([1-9][0-9]*)");

	ui_.widthBox->setValidator(new QRegExpValidator(exp));
	ui_.cutHeightBox->setValidator(new QRegExpValidator(exp));

	QObject::connect(ui_.cancelButton, &QPushButton::clicked, this, &QDialog::close);
	QObject::connect(ui_.startButton, &QPushButton::clicked, this, &CutImageDialog::start);
	QObject::connect(ui_.browseButton, &QPushButton::clicked, this, &CutImageDialog::browse);

	QFile loadFile("CutImageConfig");

	if (loadFile.open(QIODevice::ReadOnly))
	{
		QJsonDocument loadDoc = QJsonDocument::fromJson(loadFile.readAll());

		QJsonObject configObject = loadDoc.object();

		ui_.outDirEdit->setText(configObject["outdir"].toString());
		ui_.widthBox->lineEdit()->setText(QString::number(configObject["width"].toInt()));
		ui_.cutHeightBox->lineEdit()->setText(QString::number(configObject["cutHeight"].toInt()));
		ui_.extraHeightModeBox->setCurrentIndex(configObject["extraHeightMode"].toInt());
		ui_.toleranceBox->setValue(configObject["tolerance"].toInt());
		ui_.jpegQualityBox->setValue(configObject["jpegQuality"].toInt());
	}

	cancel_.store(0);
}

CutImageDialog::~CutImageDialog()
{
}

void CutImageDialog::closeEvent(QCloseEvent* e)
{
	ui_.cancelButton->setEnabled(false);

	cancel_.store(1);
	f_.waitForFinished();

	QDialog::closeEvent(e);
}

void CutImageDialog::browse()
{
	ui_.outDirEdit->setText(QFileDialog::getExistingDirectory(this));
}

void CutImageDialog::start()
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

	settings_.cutHeight = QVariant(ui_.cutHeightBox->currentText()).toInt(&ok);

	if (!ok || settings_.cutHeight < 10 || settings_.cutHeight > 10000) {
		ui_.cutHeightBox->lineEdit()->selectAll();
		ui_.cutHeightBox->setFocus();
		return;
	}

	settings_.extraHeightMode = ui_.extraHeightModeBox->currentIndex();

	settings_.tolerance = ui_.toleranceBox->value();

	settings_.jpegQuality = ui_.jpegQualityBox->value();

	QFile saveFile("CutImageConfig");

	if (saveFile.open(QIODevice::WriteOnly))
	{
		QJsonObject configObject;

		configObject["outdir"] = ui_.outDirEdit->text();
		configObject["width"] = settings_.width;
		configObject["cutHeight"] = settings_.cutHeight;
		configObject["extraHeightMode"] = settings_.extraHeightMode;
		configObject["tolerance"] = settings_.tolerance;
		configObject["jpegQuality"] = settings_.jpegQuality;

		QJsonDocument saveDoc(configObject);

		saveFile.write(saveDoc.toJson());
	}

	ui_.settingGroup->setEnabled(false);

	ui_.startButton->setEnabled(false);

	ui_.progressBar->setVisible(true);

	f_ = QtConcurrent::run(this, &CutImageDialog::proc);
}

void CutImageDialog::updateProgress(int v)
{
	ui_.progressBar->setValue(v);
}

void CutImageDialog::handleError(int ec)
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

void CutImageDialog::handleFinish(const QString& outputPath)
{
	QMessageBox msgBox(this);

	msgBox.setText(u8"操作已完成");

	QPushButton* open = msgBox.addButton(u8"打开文件夹", QMessageBox::AcceptRole);
	QPushButton* ok = msgBox.addButton(u8"确定", QMessageBox::AcceptRole);

	msgBox.setDefaultButton(ok);

	msgBox.exec();

	if (msgBox.clickedButton() == open)
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
	}

	done(1);
}

void CutImageDialog::proc()
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

		if (!boost::iequals(branch.wstring(), lastBranch.wstring()))
		{
			int ec = cutImages(QString::fromStdWString(outputPath.wstring()), lastIndex, i);

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
			int ec = cutImages(QString::fromStdWString(outputPath.wstring()), lastIndex, list_.count());

			if (ec != ec_success)
			{
				QMetaObject::invokeMethod(this, "handleError", Qt::QueuedConnection, Q_ARG(int, ec));
				return;
			}

			QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection, Q_ARG(int, 100));
		}
	}

	QMetaObject::invokeMethod(this, "handleFinish", Qt::QueuedConnection, Q_ARG(QString, QString::fromStdWString(outputPath.wstring())));
}

int CutImageDialog::cutImages(const QString& outputPath, int first, int last)
{
	QList<QImage> images;

	int ec = loadImages(images, first, last);

	if (ec != ec_success) {
		return ec;
	}

	const int boardHeight = 30000;

	QImage cuttingBoard(settings_.width, boardHeight, QImage::Format_RGB32);

	int imageIndex = 0;
	int imageY = 0;

	int remain = 0;

	QString fullOutputPath = makeOutputPath(outputPath, relativePaths_[first]);
	int number = 1;

	for (;;)
	{
		if (cancel_.load()) {
			return ec_cancel;
		}

		int h = feedCuttingBoard(cuttingBoard, remain, images, imageIndex, imageY);

		if (remain > 0) {
			h += remain;
			remain = 0;
		}

		if (h == 0) {
			break;
		}

		if (h <= settings_.cutHeight)
		{
			QImage image = cuttingBoard.copy(0, 0, settings_.width, h);

			if (!image.save(generateFilename(fullOutputPath, number++), "JPG", settings_.jpegQuality)) {
				return ec_save_error;
			}

			break;
		}

		int y = 0;

		for (;;)
		{
			if (cancel_.load()) {
				return ec_cancel;
			}

			int cy = findCuttableLine(cuttingBoard, y, y + settings_.cutHeight);

			if (cy - y >= 20)
			{
				int hh = cy - y + 1;

				QImage image = cuttingBoard.copy(0, y, settings_.width, hh);

				if (!image.save(generateFilename(fullOutputPath, number++), "JPG", settings_.jpegQuality)) {
					return ec_save_error;
				}

				y += hh;
				h -= hh;
			}
			else if (settings_.extraHeightMode == 0)
			{
				int y1 = y + settings_.cutHeight;
				int y2 = y + h;

				int cy = findCuttableLine(cuttingBoard, y1, y2, 0);

				if (cy > y1 && cy < y2)
				{
					int hh = cy - y + 1;

					QImage image = cuttingBoard.copy(0, y, settings_.width, hh);

					if (!image.save(generateFilename(fullOutputPath, number++), "JPG", settings_.jpegQuality)) {
						return ec_save_error;
					}

					y += hh;
					h -= hh;
				}
				else if (y == 0)
				{
					int hh = h;

					QImage image = cuttingBoard.copy(0, y, settings_.width, hh);

					if (!image.save(generateFilename(fullOutputPath, number++), "JPG", settings_.jpegQuality)) {
						return ec_save_error;
					}

					y += hh;
					h -= hh;
				}
				else
				{
					scrollCuttingBoard(cuttingBoard, y);
					remain = h;
					break;
				}
			}
			else
			{
				int hh = settings_.cutHeight;

				QImage image = cuttingBoard.copy(0, y, settings_.width, hh);

				if (!image.save(generateFilename(fullOutputPath, number++), "JPG", settings_.jpegQuality)) {
					return ec_save_error;
				}

				y += hh;
				h -= hh;
			}

			if (h <= settings_.cutHeight)
			{
				scrollCuttingBoard(cuttingBoard, y);
				remain = h;
				break;
			}
		}
	}

	return ec_success;
}

int CutImageDialog::feedCuttingBoard(QImage& board, int boardY, QList<QImage>& images, int& imageIndex, int& imageY)
{
	int result = 0;

	int boardSpace = board.height() - boardY;

	while (imageIndex < images.count())
	{
		QImage& image = images[imageIndex];

		const int h = image.height() - imageY;
		const int h2 = std::min(boardSpace, h);

		QPainter painter(&board);
		painter.drawImage(0, boardY, image, 0, imageY, -1, h2);
		painter.end();

		boardY += h2;
		imageY += h2;
		boardSpace -= h2;
		result += h2;

		if (imageY == image.height()) {
			++imageIndex;
			imageY = 0;
		}
		else {
			assert(boardSpace == 0);
			break;
		}
	}

	return result;
}

void CutImageDialog::scrollCuttingBoard(QImage& board, int n)
{
	QImage image = board.copy(0, n, board.width(), board.height() - n);

	QPainter painter(&board);
	painter.drawImage(0, 0, image);
	painter.end();
}

int CutImageDialog::loadImages(QList<QImage>& images, int first, int last)
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

int CutImageDialog::findCuttableLine(QImage& image, int first, int last, int order)
{
	if (order)
	{
		for (int y = last - 1; y >= first; --y)
		{
			if (isCuttable(image, y)) {
				return y;
			}
		}
	}
	else
	{
		for (int y = first; y < last; ++y)
		{
			if (isCuttable(image, y)) {
				return y;
			}
		}
	}

	return -1;
}

bool CutImageDialog::isCuttable(QImage& image, int y)
{
	QRgb c = image.pixel(0, y);

	int maxDiff = 0;

	for (int x = 1; x < image.width(); ++x)
	{
		QRgb c2 = image.pixel(x, y);

		int diff = sqrt((qRed(c2) - qRed(c)) * (qRed(c2) - qRed(c))
			+ (qGreen(c2) - qGreen(c)) * (qGreen(c2) - qGreen(c))
			+ (qBlue(c2) - qBlue(c)) * (qBlue(c2) - qBlue(c)));

		if (diff > maxDiff) {
			maxDiff = diff;
		}
	}

	if (maxDiff <= settings_.tolerance * 0.01 * 441.67295593) {
		return true;
	}

	return false;
}

QString CutImageDialog::generateFilename(const QString& base, int number)
{
	fs::path p = base.toStdWString();

	char buf[32];

	sprintf(buf, "%04d.jpg", number);

	p = p / buf;

	return QString::fromStdWString(p.wstring());
}

QString CutImageDialog::makeOutputPath(const QString& basePath, const QString& relativePath)
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

	return QString::fromStdWString(p.wstring());
}

QList<QString> CutImageDialog::removeSameBasePaths(const QList<QString>& list)
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

