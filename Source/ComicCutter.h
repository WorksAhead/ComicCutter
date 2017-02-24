#ifndef COMICCUTTER_HEADER_
#define COMICCUTTER_HEADER_

#include <QtWidgets/QMainWindow>

#include "ui_ComicCutter.h"

class ComicCutter : public QMainWindow {
private:
	Q_OBJECT

public:
	ComicCutter(QWidget *parent = 0);

public Q_SLOTS:
	void onDropFiles(int);

public:
	virtual void dragEnterEvent(QDragEnterEvent*);
	virtual void dropEvent(QDropEvent*);

private:
	Ui::ComicCutterClass ui_ ;
};


#endif // COMICCUTTER_HEADER_

