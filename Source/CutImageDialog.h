#ifndef CUTIMAGESDIALOG_HEADER_
#define CUTIMAGESDIALOG_HEADER_

#include "CutImageSettings.h"
#include "ui_CutImageDialog.h"

#include <QDialog>
#include <QtConcurrent>
#include <QAtomicInt>
#include <QList>
#include <QString>
#include <QImage>

class CutImageDialog : public QDialog {
private:
	Q_OBJECT

public:
	CutImageDialog(QList<QString>&& list, QWidget* parent = 0);
	~CutImageDialog();

public:
	virtual void closeEvent(QCloseEvent*);

private Q_SLOTS:
	void browse();
	void start();
	void updateProgress(int);
	void handleError(int);

private:
	void proc();

	int cutImages(const QString&, int, int);
	int feedCuttingBoard(QImage&, int, QList<QImage>&, int&, int&);
	void scrollCuttingBoard(QImage&, int);
	int loadImages(QList<QImage>&, int, int);
	int findCuttableLine(QImage&, int, int, int = 1);
	bool isCuttable(QImage&, int);
	QString generateFilename(const QString&, int);
	QString makeOutputPath(const QString&, const QString&);
	QList<QString> removeSameBasePaths(const QList<QString>&);

private:
	Ui::CutImageDialog ui_;

	QList<QString> list_;
	QList<QString> relativePaths_;

	CutImageSettings settings_;

	QFuture<void> f_;
	QAtomicInt cancel_;
};

#endif // CUTIMAGESDIALOG_HEADER_

