#ifndef IMAGEVIEWERWIDGET_HEADER_
#define IMAGEVIEWERWIDGET_HEADER_

#include <QWidget>
#include <QImage>
#include <QPixmap>
#include <QList>

class ManualCutImageWidget : public QWidget {
private:
	Q_OBJECT

public:
	ManualCutImageWidget(QWidget* parent = 0);
	~ManualCutImageWidget();

	void setImage(const QImage&);

	void setCutHeight(int);

	QList<QImage> resultImages();

	QList<QRect> resultRects();

Q_SIGNALS:
	void maxLineChanged(int);

public Q_SLOTS:
	void undo();
	void undoAll();

protected:
	virtual void resizeEvent(QResizeEvent*);

	virtual void mousePressEvent(QMouseEvent*);
	virtual void mouseReleaseEvent(QMouseEvent*);
	virtual void mouseMoveEvent(QMouseEvent*);

	virtual void paintEvent(QPaintEvent*);

private:
	int screenToImage(int);
	int imageToScreen(int);

private:
	QImage image_;

	QPixmap pixmap_;
	int cutHeight_;

	QPixmap scaledPixmap_;

	int line_;

	QList<int> lineList_;
};

#endif // IMAGEVIEWERWIDGET_HEADER_

