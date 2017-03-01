#ifndef JOINIMAGEDIALOG_HEADER_
#define JOINIMAGEDIALOG_HEADER_

#include "JoinImageSettings.h"
#include "ui_JoinImageDialog.h"

#include <QDialog>
#include <QtConcurrent>
#include <QAtomicInt>
#include <QList>
#include <QString>
#include <QImage>

class JoinImageDialog : public QDialog {
private:
	Q_OBJECT

public:
	JoinImageDialog(QList<QString>&& list, QWidget* parent = 0);
	~JoinImageDialog();

public:
	virtual void keyPressEvent(QKeyEvent*);
	virtual void closeEvent(QCloseEvent*);

private Q_SLOTS:
	void browse();
	void start();
	void updateProgress(int);
	void handleError(int);
	void handleFinish(const QString&);

private:
	void proc();

	int joinImages(const QString&, int, int);
	int feedCuttingBoard(QImage&, int, QList<QImage>&, int&);
	int loadImages(QList<QImage>&, int, int);
	QString generateFilename(const QString&, int);
	QString makeOutputPath(const QString&, const QString&);
	QList<QString> removeSameBasePaths(const QList<QString>&);

private:
	Ui::JoinImageDialog ui_;

	QList<QString> list_;
	QList<QString> relativePaths_;

	JoinImageSettings settings_;

	QFuture<void> f_;
	QAtomicInt cancel_;
};

#endif // JOINIMAGEDIALOG_HEADER_

