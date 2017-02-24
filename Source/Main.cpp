#include "ComicCutter.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	ComicCutter w;
	w.show();
	return a.exec();
}
