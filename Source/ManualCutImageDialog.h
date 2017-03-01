#ifndef MANUALCUTIMAGEDIALOG_HEADER_
#define MANUALCUTIMAGEDIALOG_HEADER_

#include "ui_ManualCutImageDialog.h"

#include <QImage>
#include <QList>

class ManualCutImageDialog : public QDialog {
private:
	Q_OBJECT

public:
	ManualCutImageDialog(const QImage& image, int cutHeight, QWidget* parent = 0);
	~ManualCutImageDialog();

	QList<QImage> result();

private Q_SLOTS:
	void ensureVisible(int);

private:
	Ui::ManualCutImageDialog ui_;
};

#endif // MANUALCUTIMAGEDIALOG_HEADER_

