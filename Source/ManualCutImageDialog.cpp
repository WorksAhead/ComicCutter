#include "ManualCutImageDialog.h"

#include <QBoxLayout>
#include <QLabel>

ManualCutImageDialog::ManualCutImageDialog(const QImage& image, int cutHeight, QWidget* parent)
	: QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui_.setupUi(this);

	ui_.scrollArea->setWidgetResizable(true);

	ui_.widget->setCutHeight(cutHeight);
	ui_.widget->setImage(image);

	QObject::connect(ui_.undoButton, &QPushButton::clicked, ui_.widget, &ManualCutImageWidget::undo);
	QObject::connect(ui_.undoAllButton, &QPushButton::clicked, ui_.widget, &ManualCutImageWidget::undoAll);
	QObject::connect(ui_.okButton, &QPushButton::clicked, this, &QDialog::close);

	QObject::connect(ui_.widget, &ManualCutImageWidget::maxLineChanged, this, &ManualCutImageDialog::ensureVisible);
}

ManualCutImageDialog::~ManualCutImageDialog()
{

}

QList<QImage> ManualCutImageDialog::result()
{
	return ui_.widget->resultImages();
}

void ManualCutImageDialog::ensureVisible(int y)
{
	ui_.scrollArea->ensureVisible(0, y, 0, 20);
}

