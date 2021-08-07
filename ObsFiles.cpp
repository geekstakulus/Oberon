/*
* Copyright 2021 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the Oberon+ parser/compiler library.
*
* The following is the license that applies to this copy of the
* library. For a license to use the library under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "ObsDisplay.h"
#include <QDir>
#include <QDateTime>
#include <QBuffer>
#include <stdint.h>
#include <QtDebug>

#ifdef _WIN32
#define DllExport __declspec(dllexport)
#else
#define DllExport
#endif

static QFileInfoList s_files;
static QString s_root;
static QFile s_disk;

void Obs::Display::setFileSystemRoot(const QString& dirPath) // cheat so that ObsFiles need no header just for this
{
    s_root = dirPath;
}

struct FileBuffer
{
    QBuffer* d_buf;
};
typedef uint8_t CharArray[];

static inline QString getPath()
{
    QString str = s_root;
    if( str.isEmpty() )
        str = QDir::currentPath();
    return str;
}

static inline void setBuffer( FileBuffer* fb, QBuffer* b )
{
    if( fb->d_buf )
        delete fb->d_buf;
    fb->d_buf = b;
}

static bool seekSector(int sector)
{
    sector -= 0x80002;
    if( !s_disk.isOpen() )
    {
        QDir dir( getPath() );
        QStringList files = dir.entryList( QStringList() << "*.dsk", QDir::Files, QDir::Name );
        if( !files.isEmpty() )
        {
            s_disk.setFileName(files.last());
            if( !s_disk.open(QIODevice::ReadWrite) )
                qCritical() << "cannot open disk file" << s_disk.fileName();
            else
            {
                const QByteArray hdr = s_disk.read(4);
                if( hdr.size() != 4 || hdr[0] != 0x8d || hdr[1] != 0xa3 || hdr[2] != 0x1e || hdr[3] != 0x9b )
                {
                    qCritical() << "invalid disk format" << s_disk.fileName();
                    s_disk.close();
                }else
                    qDebug() << "using disk file" << s_disk.fileName();
            }
        }else
            qCritical() << "cannot find disk file";
    }
    return s_disk.seek(sector);
}

static QList<FileBuffer> s_buffers;

extern "C"
{

DllExport int ObsFiles_setRootPath( const char* path )
{
    s_root = QString::fromLatin1(path);
    return 0;
}

DllExport int ObsFiles_listFiles()
{
    QString str = getPath();
    QDir dir( str );
    s_files = dir.entryInfoList( QDir::Files | QDir::Readable | QDir::Writable );
    return s_files.size();
}

DllExport const char* ObsFiles_fileName( int i )
{
    static QByteArray name;
    name = s_files[i].fileName().left(31).toUtf8();
    return name.constData();
}

DllExport uint32_t ObsFiles_fileSize( int i )
{
    return s_files[i].size();
}

DllExport uint32_t ObsFiles_fileTime( int i )
{
    return s_files[i].created().toTime_t();
}

DllExport int ObsFiles_openFile( CharArray filename, FileBuffer* fb )
{
    QDir dir( getPath() );
    const QString path = dir.absoluteFilePath( QString::fromLatin1((char*)filename) );
    if( !QFileInfo(path).isFile() )
        return false;
    QFile f( path );
    if( f.exists() )
    {
        if( !f.open(QIODevice::ReadOnly) )
        {
            qWarning() << "*** could not open for reading" << f.fileName();
            return false;
        }
        setBuffer(fb,new QBuffer() );
        fb->d_buf->setData(f.readAll());
        fb->d_buf->open( QIODevice::ReadWrite );
        return true;
    }else
        return false;
}

DllExport int ObsFiles_newFile( FileBuffer* fb )
{
    setBuffer(fb,new QBuffer() );
    fb->d_buf->open( QIODevice::ReadWrite );
    return true;
}

DllExport void ObsFiles_freeFile( FileBuffer* fb )
{
    setBuffer(fb,0);
}

DllExport int ObsFiles_saveFile( CharArray filename, FileBuffer* fb )
{
    QDir dir( getPath() );
    QFile f( dir.absoluteFilePath( QString::fromLatin1((char*)filename)) );
    if( fb->d_buf )
    {
        if( !f.open(QIODevice::WriteOnly) )
            qWarning() << "*** could not open for writing" << f.fileName();
        fb->d_buf->close();
        setBuffer(fb,new QBuffer() );
        f.write(fb->d_buf->data());
        fb->d_buf->open( QIODevice::ReadWrite );
        return true;
    }else
        return false;
}

DllExport int ObsFiles_removeFile( CharArray filename )
{
    QDir dir( getPath() );
    return dir.remove(QString::fromLatin1((char*)filename));
}

DllExport int ObsFiles_renameFile( CharArray oldName, CharArray newName )
{
    QDir dir( getPath() );
    const QString old = QString::fromLatin1((const char*)oldName);
    const QString _new = QString::fromLatin1((const char*)newName);
    if( !QFileInfo(dir.absoluteFilePath(old)).exists() )
        return false; // not found
    QFileInfo info(dir.absoluteFilePath(_new));
    if( info.exists() )
    {
        dir.remove(_new);
        return dir.rename(old,_new);
    }else
        return dir.rename(old,_new);
}

DllExport uint32_t ObsFiles_length( FileBuffer* fb )
{
    if( fb->d_buf )
        return fb->d_buf->size();
    else
        return 0;
}

DllExport int ObsFiles_setPos( FileBuffer* fb, int pos )
{
    if( fb->d_buf )
    {
        if( pos < 0 ) // it happens a few times that -1 instead of 0 is passed; according to Oberon book should always be >= 0
            pos = 0;
        if( !fb->d_buf->seek(pos) )
        {
            qWarning() << "*** could not seek to" << pos << fb->d_buf->pos() << fb->d_buf->size();
            return false;
        }
        return true;
    }else
        return false;
}

DllExport int ObsFiles_getPos( FileBuffer* fb )
{
    if( fb->d_buf )
        return fb->d_buf->pos();
    else
        return 0;
}


DllExport int ObsFiles_atEnd( FileBuffer* fb )
{
    if( fb->d_buf )
        return fb->d_buf->atEnd();
    else
        return false;
}

DllExport int ObsFiles_writeByte( FileBuffer* fb, uint32_t byte )
{
    if( fb->d_buf )
    {
        return fb->d_buf->putChar( (char) (byte & 0xff) );
    }else
        return false;
}

DllExport uint32_t ObsFiles_readByte( FileBuffer* fb )
{
    if( fb->d_buf )
    {
        char ch;
        if( fb->d_buf->getChar( &ch ) )
            return (quint8)ch;
        else
            return 0;
    }else
        return 0;
}

enum { SECLEN = 1024 };

DllExport void ObsFiles_readSector(int sector, uint8_t* data )
{
    if( !seekSector(sector) )
    {
        ::memset(data,0,SECLEN);
    }else
    {
        const int res = s_disk.read((char*)data,SECLEN);
        if( res != SECLEN )
        {
            if( res < 0 )
                ::memset(data,0,SECLEN);
            else
                ::memset(data+res,0,SECLEN-res);
        }
    }
}

DllExport void ObsFiles_writeSector(int sector, uint8_t* data )
{
    if( seekSector(sector) )
    {
        const int res = s_disk.write((char*)data,SECLEN);
        if( res != SECLEN )
            qCritical() << "error writing to disk file" << s_disk.fileName();
    }
}



}
