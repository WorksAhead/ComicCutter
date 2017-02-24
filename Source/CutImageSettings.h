#ifndef CUTIMAGESETTINGS_HEADER_
#define CUTIMAGESETTINGS_HEADER_

#include <QString>

struct CutImageSettings {

	QString outputDir;

	int width;
	int cutHeight;
	int extraHeightMode;
	int tolerance;
	int jpegQuality;

};

#endif // CUTIMAGESETTINGS_HEADER_

