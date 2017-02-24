#ifndef COLLECTIMAGESDIALOG_HEADER_
#define COLLECTIMAGESDIALOG_HEADER_

#include "ui_DropFilesDialog.h"

#include <QtConcurrent>
#include <QAtomicInt>
#include <QList>
#include <QUrl>

class DropFilesDialog : public QDialog {
private:
	Q_OBJECT

public:
	DropFilesDialog(const QList<QUrl>&, QWidget* parent = 0);
	~DropFilesDialog();

	QList<QString>& list();

public:
	virtual void closeEvent(QCloseEvent*);

private Q_SLOTS:
	void updateProgress(int);

private:
	void proc();

private:
	Ui::DropFilesDialog ui_;

	QList<QUrl> urls_;

	QList<QString> list_;

	QFuture<void> f_;
	QAtomicInt quit_;
};

#endif // COLLECTIMAGESDIALOG_HEADER_

