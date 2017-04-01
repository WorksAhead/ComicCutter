#include "ManualCutImageWidget.h"

#include <QGridLayout>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

#include <algorithm>

ManualCutImageWidget::ManualCutImageWidget(QWidget* parent) : QWidget(parent)
{
	line_ = -1;
}

ManualCutImageWidget::~ManualCutImageWidget()
{
}

void ManualCutImageWidget::setImage(const QImage& image)
{
	image_ = image;

	pixmap_ = QPixmap::fromImage(image_);

	scaledPixmap_ = pixmap_.scaledToWidth(width(), Qt::SmoothTransformation);

	setFixedHeight(scaledPixmap_.height());
}

void ManualCutImageWidget::setCutHeight(int cutHeight)
{
	cutHeight_ = cutHeight;
}

void ManualCutImageWidget::undo()
{
	if (lineList_.count())
	{
		lineList_.pop_back();

		repaint();
	}
}

void ManualCutImageWidget::undoAll()
{
	if (lineList_.count())
	{
		lineList_.clear();

		repaint();
	}
}

QList<QRect> ManualCutImageWidget::resultRects()
{
	QList<int> lineList = lineList_;

	std::sort(lineList.begin(), lineList.end());

	QList<QRect> result;

	int lastY = 0;

	for (int y : lineList)
	{
		QRect rect(0, lastY, image_.width(), y - lastY + 1);

		if (rect.height() > 0) {
			result.append(rect);
		}

		lastY = y + 1;
	}

	result.append(QRect(0, lastY, image_.width(), image_.height() - lastY));

	return result;
}

void ManualCutImageWidget::resizeEvent(QResizeEvent* e)
{
	scaledPixmap_ = pixmap_.scaledToWidth(e->size().width(), Qt::SmoothTransformation);

	setFixedHeight(scaledPixmap_.height());
}

void ManualCutImageWidget::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
	{
		line_ = e->pos().y();

		repaint();
	}
}

void ManualCutImageWidget::mouseReleaseEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
	{
		int line = screenToImage(line_);

		if (line >= 0 && line < image_.height())
		{
			bool overlap = false;
			bool bigger = false;

			for (int y : lineList_)
			{
				if (line == y) {
					overlap = true;
				}

				if (line < y) {
					bigger = true;
				}
			}

			if (!overlap)
			{
				lineList_.append(line);

				if (!bigger) {
					Q_EMIT maxLineChanged(imageToScreen(line + cutHeight_));
				}
			}
		}

		line_ = -1;

		repaint();
	}
	else if (e->button() == Qt::RightButton)
	{		
		int maxLine = 0;

		for (int y : lineList_)
		{
			if (y > maxLine)
			{
				maxLine = y;
			}
		}

		int line = maxLine + cutHeight_;

		if (line < image_.height())
		{
			lineList_.append(line);

			Q_EMIT maxLineChanged(imageToScreen(line + cutHeight_));

			repaint();
		}
	}
}

void ManualCutImageWidget::mouseMoveEvent(QMouseEvent* e)
{
	if (line_ != -1)
	{
		line_ = e->pos().y();

		repaint();
	}
}

void ManualCutImageWidget::paintEvent(QPaintEvent* e)
{
	QPainter painter(this);

	painter.drawPixmap(0, 0, scaledPixmap_);

	int maxLine = -1;

	for (int line : lineList_)
	{
		if (line > maxLine) {
			maxLine = line;
		}

		line = imageToScreen(line);

		painter.setPen(QColor(255, 0, 0));
		painter.drawLine(0, line, width() - 1, line);
	}

	if (maxLine >= -1)
	{
		int y = imageToScreen(maxLine + 1);
		int h = imageToScreen(cutHeight_);

		painter.fillRect(0, y, width() - 1, h, QColor(0, 255, 0, 100));
	}

	QList<QRect> rects = resultRects();

	int n = 1;

	for (QRect rect : rects)
	{
		rect.setTop(imageToScreen(rect.top()));
		rect.setBottom(imageToScreen(rect.bottom()));
		rect.setLeft(0);
		rect.setRight(width() - 1);

		QString text = QString("%1").arg(n++);

		QFont font("Georgia", 24, QFont::Bold);

		QFontMetrics fm(font);

		painter.setFont(font);

		QRect textRect = fm.boundingRect(rect.adjusted(5, 5, -5, -5), Qt::AlignLeft|Qt::TextWordWrap, text);

		painter.fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(255, 0, 0));

		painter.setPen(QColor(250, 250, 250));

		painter.drawText(textRect, text);
	}

	if (line_ != -1)
	{
		int h = screenToImage(line_);

		for (QRect rect : rects)
		{
			if (h >= rect.top() && h <= rect.bottom())
			{
				h = h - rect.top();

				painter.setPen(QColor(255, 0, 255));

				painter.drawLine(0, line_, width() - 1, line_);

				QString text = QString("%1").arg(h);

				QFont font("Segoe UI", 10);

				QFontMetrics fm(font);

				painter.setFont(font);

				QRect textRect = fm.boundingRect(text);

				textRect.moveLeft((width() - textRect.width() + 4) / 2);
				textRect.moveBottom(line_ - 2);

				painter.fillRect(textRect.adjusted(-2, -2, 2, 2), QColor(255, 0, 255));

				painter.setPen(QColor(250, 250, 250));

				painter.drawText(textRect, text);

				break;
			}
		}
	}
}

int ManualCutImageWidget::screenToImage(int y)
{
	double scale = (double)scaledPixmap_.height() / (double)pixmap_.height();
	return floor(y / scale + 0.5);
}

int ManualCutImageWidget::imageToScreen(int y)
{
	double scale = (double)scaledPixmap_.height() / (double)pixmap_.height();
	return floor(y * scale + 0.5);
}

