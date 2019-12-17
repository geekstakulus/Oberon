/*
* Copyright 2019 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the Oberon parser/compiler library.
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

#include "ObnLjEditor.h"
#include "ObnHighlighter.h"
#include "ObCodeModel.h"
#include "ObFileCache.h"
#include "ObLuaGen.h"
#include "ObLjLib.h"
#include "ObAst.h"
#include "ObAstEval.h"
#include <LjTools/Engine2.h>
#include <LjTools/Terminal2.h>
#include <LjTools/BcViewer.h>
#include <LjTools/LuaJitEngine.h>
#include <QtDebug>
#include <QDockWidget>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QSettings>
#include <QShortcut>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QBuffer>
#include <GuiTools/AutoMenu.h>
#include <GuiTools/CodeEditor.h>
#include <GuiTools/AutoShortcut.h>
using namespace Ob;
using namespace Lua;

static LjEditor* s_this = 0;
static void report(QtMsgType type, const QString& message )
{
    if( s_this )
    {
        switch(type)
        {
        case QtDebugMsg:
            s_this->logMessage(QLatin1String("INF: ") + message);
            break;
        case QtWarningMsg:
            s_this->logMessage(QLatin1String("WRN: ") + message);
            break;
        case QtCriticalMsg:
        case QtFatalMsg:
            s_this->logMessage(QLatin1String("ERR: ") + message, true);
            break;
        }
    }
}
static QtMessageHandler s_oldHandler = 0;
void messageHander(QtMsgType type, const QMessageLogContext& ctx, const QString& message)
{
    if( s_oldHandler )
        s_oldHandler(type, ctx, message );
    report(type,message);
}

LjEditor::LjEditor(QWidget *parent)
    : QMainWindow(parent),d_lock(false)
{
    s_this = this;

    d_mdl = new CodeModel(this);
    d_mdl->setSenseExt(true);
    d_mdl->setSynthesize(false);
    d_mdl->setTrackIds(false);

    d_lua = new Engine2(this);
    d_lua->addStdLibs();
    d_lua->addLibrary(Engine2::PACKAGE);
    d_lua->addLibrary(Engine2::IO);
    d_lua->addLibrary(Engine2::DBG);
    d_lua->addLibrary(Engine2::BIT);
    d_lua->addLibrary(Engine2::JIT);
    d_lua->addLibrary(Engine2::OS);
    LjLib::install(d_lua->getCtx());
    QFile obnlj( ":/scripts/obnlj.lua" );
    obnlj.open(QIODevice::ReadOnly);
    if( !d_lua->addSourceLib( obnlj.readAll(), "obnlj" ) )
        qCritical() << "compiling obnlj:" << d_lua->getLastError();
    Engine2::setInst(d_lua);

    d_eng = new JitEngine(this);

    d_edit = new CodeEditor(this);
    d_hl = new Highlighter( d_edit->document() );
    d_edit->updateTabWidth();

    setDockNestingEnabled(true);
    setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
    setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
    setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
    setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );

    createTerminal();
    createDumpView();
    createMenu();

    setCentralWidget(d_edit);

    s_oldHandler = qInstallMessageHandler(messageHander);

    QSettings s;

    if( s.value("Fullscreen").toBool() )
        showFullScreen();
    else
        showMaximized();

    const QVariant state = s.value( "DockState" );
    if( !state.isNull() )
        restoreState( state.toByteArray() );


    connect(d_edit, SIGNAL(modificationChanged(bool)), this, SLOT(onCaption()) );
    connect(d_bcv,SIGNAL(sigGotoLine(int)),this,SLOT(onGotoLnr(int)));
    connect(d_edit,SIGNAL(cursorPositionChanged()),this,SLOT(onCursor()));
    connect(d_eng,SIGNAL(sigPrint(QString,bool)), d_term, SLOT(printText(QString,bool)) );
}

LjEditor::~LjEditor()
{

}

static bool preloadLib( Ast::Model& mdl, const QByteArray& name )
{
    QFile f( QString(":/oakwood/%1.Def" ).arg(name.constData() ) );
    if( !f.open(QIODevice::ReadOnly) )
    {
        qCritical() << "unknown preload" << name;
        return false;
    }
    mdl.addPreload( name, f.readAll() );
    return true;
}

void LjEditor::loadFile(const QString& path)
{
    d_edit->loadFromFile(path);
    QDir::setCurrent(QFileInfo(path).absolutePath());
    onCaption();

    onDumpSrc();
}

void LjEditor::logMessage(const QString& str, bool err)
{
    d_term->printText(str,err);
}

void LjEditor::closeEvent(QCloseEvent* event)
{
    QSettings s;
    s.setValue( "DockState", saveState() );
    event->setAccepted(checkSaved( tr("Quit Application")));
}

void LjEditor::createTerminal()
{
    QDockWidget* dock = new QDockWidget( tr("Terminal"), this );
    dock->setObjectName("Terminal");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable );
    d_term = new Terminal2(dock, d_lua);
    dock->setWidget(d_term);
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    new Gui::AutoShortcut( tr("CTRL+SHIFT+C"), this, d_term, SLOT(onClear()) );
}

void LjEditor::createDumpView()
{
    QDockWidget* dock = new QDockWidget( tr("Bytecode"), this );
    dock->setObjectName("Bytecode");
    dock->setAllowedAreas( Qt::AllDockWidgetAreas );
    dock->setFeatures( QDockWidget::DockWidgetMovable );
    d_bcv = new BcViewer(dock);
    dock->setWidget(d_bcv);
    addDockWidget( Qt::RightDockWidgetArea, dock );
}

void LjEditor::createMenu()
{
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_edit, true );
    pop->addCommand( "New", this, SLOT(onNew()), tr("CTRL+N"), false );
    pop->addCommand( "Open...", this, SLOT(onOpen()), tr("CTRL+O"), false );
    pop->addCommand( "Save", this, SLOT(onSave()), tr("CTRL+S"), false );
    pop->addCommand( "Save as...", this, SLOT(onSaveAs()) );
    pop->addSeparator();
    pop->addCommand( "Execute LuaJIT", this, SLOT(onRun()), tr("CTRL+E"), false );
    pop->addCommand( "Execute test VM", this, SLOT(onRun2()), tr("CTRL+SHIFT+E"), false );
    pop->addCommand( "Dump", this, SLOT(onDumpBin()), tr("CTRL+D"), false );
    pop->addCommand( "Export binary...", this, SLOT(onExportBc()) );
    pop->addCommand( "Export assembler...", this, SLOT(onExportAsm()) );
    pop->addSeparator();
    pop->addCommand( "Undo", d_edit, SLOT(handleEditUndo()), tr("CTRL+Z"), true );
    pop->addCommand( "Redo", d_edit, SLOT(handleEditRedo()), tr("CTRL+Y"), true );
    pop->addSeparator();
    pop->addCommand( "Cut", d_edit, SLOT(handleEditCut()), tr("CTRL+X"), true );
    pop->addCommand( "Copy", d_edit, SLOT(handleEditCopy()), tr("CTRL+C"), true );
    pop->addCommand( "Paste", d_edit, SLOT(handleEditPaste()), tr("CTRL+V"), true );
    pop->addSeparator();
    pop->addCommand( "Find...", d_edit, SLOT(handleFind()), tr("CTRL+F"), true );
    pop->addCommand( "Find again", d_edit, SLOT(handleFindAgain()), tr("F3"), true );
    pop->addCommand( "Replace...", d_edit, SLOT(handleReplace()), tr("CTRL+R"), true );
    pop->addSeparator();
    pop->addCommand( "&Goto...", d_edit, SLOT(handleGoto()), tr("CTRL+G"), true );
    pop->addCommand( "Go Back", d_edit, SLOT(handleGoBack()), tr("ALT+Left"), true );
    pop->addCommand( "Go Forward", d_edit, SLOT(handleGoForward()), tr("ALT+Right"), true );
    pop->addSeparator();
    pop->addCommand( "Indent", d_edit, SLOT(handleIndent()) );
    pop->addCommand( "Unindent", d_edit, SLOT(handleUnindent()) );
    pop->addCommand( "Fix Indents", d_edit, SLOT(handleFixIndent()) );
    pop->addCommand( "Set Indentation Level...", d_edit, SLOT(handleSetIndent()) );
    pop->addSeparator();
    pop->addCommand( "Print...", d_edit, SLOT(handlePrint()), tr("CTRL+P"), true );
    pop->addCommand( "Export PDF...", d_edit, SLOT(handleExportPdf()), tr("CTRL+SHIFT+P"), true );
    pop->addSeparator();
    pop->addCommand( "Set &Font...", d_edit, SLOT(handleSetFont()) );
    pop->addCommand( "Show &Linenumbers", d_edit, SLOT(handleShowLinenumbers()) );
    pop->addCommand( "Show Fullscreen", this, SLOT(onFullScreen()) );
    pop->addSeparator();
    pop->addAction(tr("Quit"),qApp,SLOT(quit()), tr("CTRL+Q") );

    new QShortcut(tr("CTRL+Q"),this,SLOT(close()));
    new Gui::AutoShortcut( tr("CTRL+O"), this, this, SLOT(onOpen()) );
    new Gui::AutoShortcut( tr("CTRL+N"), this, this, SLOT(onNew()) );
    new Gui::AutoShortcut( tr("CTRL+O"), this, this, SLOT(onOpen()) );
    new Gui::AutoShortcut( tr("CTRL+S"), this, this, SLOT(onSave()) );
    new Gui::AutoShortcut( tr("CTRL+E"), this, this, SLOT(onRun()) );
    new Gui::AutoShortcut( tr("CTRL+SHIFT+E"), this, this, SLOT(onRun2()) );
    new Gui::AutoShortcut( tr("CTRL+SHIFT+D"), this, this, SLOT(onDumpBin()) );
    new Gui::AutoShortcut( tr("CTRL+D"), this, this, SLOT(onDumpSrc()) );
}

void LjEditor::onDumpBin()
{
    ENABLED_IF(true);
    compile(false);
}

void LjEditor::onDumpSrc()
{
    ENABLED_IF(true);
    compile(true);
}

void LjEditor::onRun()
{
    ENABLED_IF(true);
    compile(true);
    d_lua->executeCmd( d_luaCode, d_edit->getPath().toUtf8() );
}

void LjEditor::onRun2()
{
    ENABLED_IF(true);
    compile(true);
    QDir dir( QStandardPaths::writableLocation(QStandardPaths::TempLocation) );
    const QString path = dir.absoluteFilePath(QDateTime::currentDateTime().toString("yyMMddhhmmsszzz")+".bc");
    d_lua->saveBinary(d_luaCode, d_edit->getPath().toUtf8(),path.toUtf8());
    JitBytecode bc;
    if( bc.parse(path) )
        d_eng->run( &bc );
    dir.remove(path);
}

void LjEditor::onNew()
{
    ENABLED_IF(true);

    if( !checkSaved( tr("New File")) )
        return;

    d_edit->newFile();
    onCaption();
}

void LjEditor::onOpen()
{
    ENABLED_IF( true );

    if( !checkSaved( tr("New File")) )
        return;

    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),QString(),
                                                          tr("Oberon Files (*.Mod *.obn)") );
    if (fileName.isEmpty())
        return;

    QDir::setCurrent(QFileInfo(fileName).absolutePath());

    d_edit->loadFromFile(fileName);
    onCaption();

    compile(true);
}

void LjEditor::onSave()
{
    ENABLED_IF( d_edit->isModified() );

    if( !d_edit->getPath().isEmpty() )
        d_edit->saveToFile( d_edit->getPath() );
    else
        onSaveAs();
}

void LjEditor::onSaveAs()
{
    ENABLED_IF(true);

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                                                          QFileInfo(d_edit->getPath()).absolutePath(),
                                                          tr("Oberon Files (*.Mod *.obn)") );

    if (fileName.isEmpty())
        return;

    QDir::setCurrent(QFileInfo(fileName).absolutePath());

    if( !fileName.endsWith(".Mod",Qt::CaseInsensitive ) && !fileName.endsWith(".obn",Qt::CaseInsensitive ) )
        fileName += ".Mod";

    d_edit->saveToFile(fileName);
    onCaption();
}

void LjEditor::onCaption()
{
    if( d_edit->getPath().isEmpty() )
    {
        setWindowTitle(tr("<unnamed> - %1").arg(qApp->applicationName()));
    }else
    {
        QFileInfo info(d_edit->getPath());
        QString star = d_edit->isModified() ? "*" : "";
        setWindowTitle(tr("%1%2 - %3").arg(info.fileName()).arg(star).arg(qApp->applicationName()) );
    }
}

void LjEditor::onGotoLnr(int lnr)
{
    if( d_lock )
        return;
    d_lock = true;
    d_edit->setCursorPosition(lnr-1,0);
    d_lock = false;
}

void LjEditor::onFullScreen()
{
    CHECKED_IF(true,isFullScreen());
    QSettings s;
    if( isFullScreen() )
    {
        showMaximized();
        s.setValue("Fullscreen", false );
    }else
    {
        showFullScreen();
        s.setValue("Fullscreen", true );
    }
}

void LjEditor::onCursor()
{
    if( d_lock )
        return;
    d_lock = true;
    QTextCursor cur = d_edit->textCursor();
    const int line = cur.blockNumber() + 1;
    d_bcv->gotoLine(line);
    d_lock = false;
}

void LjEditor::onExportBc()
{
    ENABLED_IF(true);
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Binary"),
                                                          d_edit->getPath(),
                                                          tr("*.bc") );

    if (fileName.isEmpty())
        return;

    QDir::setCurrent(QFileInfo(fileName).absolutePath());

    if( !fileName.endsWith(".bc",Qt::CaseInsensitive ) )
        fileName += ".bc";
    d_lua->saveBinary(d_edit->toPlainText().toUtf8(), d_edit->getPath().toUtf8(),fileName.toUtf8());
}

void LjEditor::onExportAsm()
{
    ENABLED_IF(true);

    if( d_bcv->topLevelItemCount() == 0 )
        onDumpBin();
    if( d_bcv->topLevelItemCount() == 0 )
        return;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Assembler"),
                                                          d_edit->getPath(),
                                                          tr("*.ljasm") );

    if (fileName.isEmpty())
        return;

    QDir::setCurrent(QFileInfo(fileName).absolutePath());

    if( !fileName.endsWith(".ljasm",Qt::CaseInsensitive ) )
        fileName += ".ljasm";

    d_bcv->saveTo(fileName);
}

bool LjEditor::checkSaved(const QString& title)
{
    if( d_edit->isModified() )
    {
        switch( QMessageBox::critical( this, title, tr("The file has not been saved; do you want to save it?"),
                               QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes ) )
        {
        case QMessageBox::Yes:
            if( !d_edit->getPath().isEmpty() )
                return d_edit->saveToFile(d_edit->getPath());
            else
            {
                const QString path = QFileDialog::getSaveFileName( this, title, QString(), "Oberon Files (*.Mod *.obn)" );
                if( path.isEmpty() )
                    return false;
                QDir::setCurrent(QFileInfo(path).absolutePath());
                return d_edit->saveToFile(path);
            }
            break;
        case QMessageBox::No:
            return true;
        default:
            return false;
        }
    }
    return true;
}

void LjEditor::compile(bool asSource)
{
    QString path = d_edit->getPath();
    if( path.isEmpty() )
        path = "<unnamed>";

#define _USE_AST_MODEL
#ifdef _USE_AST_MODEL
    Ast::Model mdl;
    mdl.setSenseExt(true);
    mdl.getFc()->addFile(path,d_edit->toPlainText().toUtf8() );
    preloadLib(mdl,"In");
    preloadLib(mdl,"Out");
    preloadLib(mdl,"Files");
    preloadLib(mdl,"Input");
    preloadLib(mdl,"Math");
    preloadLib(mdl,"Strings");
    preloadLib(mdl,"Coroutines");
    preloadLib(mdl,"XYPlane");
    mdl.parseFiles(QStringList() << path );
    Ast::Model::Modules mods = mdl.getModules();
    QTextStream out(stdout);
    for( int m = 0; m < mods.size(); m++ )
        Ast::Eval::render(out,mods[m].data());
#else
    d_mdl->getFc()->addFile(path,d_edit->toPlainText().toUtf8() );
    if( d_mdl->parseFiles( QStringList() << path ) )
    {
        Q_ASSERT( d_mdl->getGlobalScope().d_mods.size() == 1 );
        const CodeModel::Module* m = d_mdl->getGlobalScope().d_mods.first();
        QFile dump( "dump.txt");
        if( !dump.open(QIODevice::WriteOnly) )
            qDebug() << "error: cannot open dump file for writing" << dump.fileName();
        QTextStream ts(&dump);
        Ob::CodeModel::dump(ts,m->d_def);

        d_hl->setEnableExt(m->d_isExt);

        if( asSource )
        {
            LuaGen gen(d_mdl);
            d_luaCode = gen.emitModule(m);
            if( !d_luaCode.isEmpty() )
            {
                QFile dump( "dump.lua");
                if( !dump.open(QIODevice::WriteOnly) )
                    qDebug() << "error: cannot open dump file for writing" << dump.fileName();
                dump.write(d_luaCode);
                if( d_lua->pushFunction(d_luaCode) )
                {
                    QByteArray code = Engine2::getBinaryFromFunc( d_lua->getCtx() );
                    d_lua->pop();
                    QBuffer buf(&code);
                    buf.open(QIODevice::ReadOnly);
                    d_bcv->loadFrom(&buf,path);
                }
            }
        }else
        {
#ifdef _GEN_BYTECODE_
            LjbcGen gen(d_mdl);
            QByteArray bc = gen.emitModule(d_mdl->getGlobalScope().d_mods.first());
            if( !bc.isEmpty() )
            {
                QBuffer buf(&bc);
                buf.open(QIODevice::ReadOnly);
                d_bcv->loadFrom(&buf,path);
            }
#endif
        }
    }
#endif
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("me@rochus-keller.ch");
    a.setOrganizationDomain("github.com/rochus-keller/Oberon");
    a.setApplicationName("ObnLjEditor");
    a.setApplicationVersion("0.5.0");
    a.setStyle("Fusion");

    LjEditor w;

    if( a.arguments().size() > 1 )
        w.loadFile(a.arguments()[1] );

    return a.exec();
}