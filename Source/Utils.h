#ifndef UTILS_HEADER_
#define UTILS_HEADER_

#include <QString>
#include <QFileInfo>

inline bool isImageFile(const QFileInfo& info)
{
	const char* extensions[] = {"jpg", "jpeg", "png", "psd", "psb"};

	if (info.isFile())
	{
		for (const char* ext : extensions)
		{
			if (QString::compare(info.suffix(), ext, Qt::CaseInsensitive) == 0) {
				return true;
			}
		}
	}

	return false;
}

inline bool isImageFile(const QString& path)
{
	return isImageFile(QFileInfo(path));
}

#endif // UTILS_HEADER_

