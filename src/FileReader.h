/**************************************************************************
 *  This file is part of QuteScoop. See README for license
 **************************************************************************/
#ifndef FILEREADER_H_
#define FILEREADER_H_

#include <QtCore>

class FileReader {
    public:
        FileReader(const QString& filename);
        virtual ~FileReader();

        bool atEnd() const;
        QString nextLine() const;
    private:
        QFile *_file;
        QTextStream *_stream;
};

#endif /*FILEREADER_H_*/
