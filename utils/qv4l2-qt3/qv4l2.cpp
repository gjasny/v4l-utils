
#include "qv4l2.h"
#include "general-tab.h"
#include "libv4l2util.h"

#include <qimage.h>
#include <qpixmap.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qpopupmenu.h>
#include <qmenubar.h>
#include <qfile.h>
#include <qfiledialog.h>
#include <qstatusbar.h>
#include <qapplication.h>
#include <qmessagebox.h>
#include <qlineedit.h>
#include <qvalidator.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <dirent.h>

#include "fileopen.xpm"

ApplicationWindow::ApplicationWindow()
    : QMainWindow( 0, "V4L2 main window", WDestructiveClose | WGroupLeader )
{
    QPixmap openIcon, saveIcon;

    fd = -1;

    videoDevice = NULL;
    sigMapper = NULL;
    QToolBar * fileTools = new QToolBar( this, "file operations" );
    fileTools->setLabel( "File Operations" );

    openIcon = QPixmap( fileopen );
    QToolButton * fileOpen
	= new QToolButton( openIcon, "Open File", QString::null,
			   this, SLOT(choose()), fileTools, "open file" );

    (void)QWhatsThis::whatsThisButton( fileTools );

    const char * fileOpenText = "<p><img source=\"fileopen\"> "
		 "Click this button to open a <em>new v4l device</em>.<br>"
		 "You can also select the <b>Open</b> command "
		 "from the <b>File</b> menu.</p>";

    QWhatsThis::add( fileOpen, fileOpenText );

    QMimeSourceFactory::defaultFactory()->setPixmap( "fileopen", openIcon );

    QPopupMenu * file = new QPopupMenu( this );
    menuBar()->insertItem( "&File", file );


    int id;
    id = file->insertItem( openIcon, "&Open...",
			   this, SLOT(choose()), CTRL+Key_O );
    file->setWhatsThis( id, fileOpenText );

    file->insertSeparator();

    file->insertItem( "&Close", this, SLOT(close()), CTRL+Key_W );

    file->insertItem( "&Quit", qApp, SLOT( closeAllWindows() ), CTRL+Key_Q );

    menuBar()->insertSeparator();

    QPopupMenu * help = new QPopupMenu( this );
    menuBar()->insertItem( "&Help", help );

    help->insertItem( "&About", this, SLOT(about()), Key_F1 );
    help->insertItem( "What's &This", this, SLOT(whatsThis()), SHIFT+Key_F1 );

    statusBar()->message( "Ready", 2000 );

    tabs = new QTabWidget(this);
    tabs->setMargin(3);

    //resize( 450, 600 );
}


ApplicationWindow::~ApplicationWindow()
{
	if (fd >= 0) ::close(fd);
}


void ApplicationWindow::setDevice(const QString &device)
{
	if (fd >= 0) ::close(fd);
	while (QWidget *page = tabs->page(0)) {
		tabs->removePage(page);
		delete page;
	}
	delete tabs;
	delete sigMapper;
	tabs = new QTabWidget(this);
	tabs->setMargin(3);
	sigMapper = new QSignalMapper(this);
	connect(sigMapper, SIGNAL(mapped(int)), this, SLOT(ctrlAction(int)));
	ctrlMap.clear();
	widgetMap.clear();
	classMap.clear();

	fd = ::open(device, O_RDONLY);
	if (fd >= 0) {
		tabs->addTab(new GeneralTab(device, fd, 4, tabs), "General");
		addTabs();
	}
	if (QWidget *current = tabs->currentPage()) {
		current->show();
	}
	tabs->show();
	tabs->setFocus();
	setCentralWidget(tabs);
}

void ApplicationWindow::selectdev(int index)
{
	setDevice(videoDevice->text(index));
}

void ApplicationWindow::add_dirVideoDevice(const char *dirname)
{
	DIR		*dir;
	struct dirent	*entry;
	const char	*vid = "video";
	const char	*rad = "radio";
	const char	*vbi = "vbi";
	char		name[512], *p;

	dir = opendir(dirname);
	if (!dir)
		return;

	strcpy(name, dirname);
	strcat(name, "/");
	p = name + strlen(name);

	entry = readdir(dir);
	while (entry) {
		if (!strncmp(entry->d_name, vid, strlen(vid)) ||
		    !strncmp(entry->d_name, rad, strlen(rad)) ||
		    !strncmp(entry->d_name, vbi, strlen(vbi))) {
			strcpy(p, entry->d_name);

			videoDevice->insertItem(name);
		}
		entry = readdir(dir);
	}
	closedir(dir);
}

void ApplicationWindow::choose()
{
	if (videoDevice)
		delete videoDevice;

	videoDevice = new QPopupMenu(this);

	add_dirVideoDevice("/dev");
	add_dirVideoDevice("/dev/v4l");

	connect(videoDevice, SIGNAL(activated(int)), this, SLOT(selectdev(int)));

	videoDevice->show();
	videoDevice->setFocus();
}

void ApplicationWindow::closeEvent( QCloseEvent* ce )
{
	ce->accept();
}

bool ApplicationWindow::doIoctl(QString descr, unsigned cmd, void *arg)
{
	statusBar()->clear();
	int err = ioctl(fd, cmd, arg);

	if (err == -1) {
		QString s = strerror(errno);
		statusBar()->message(descr + ": " + s, 10000);
	}
	return err != -1;
}

void ApplicationWindow::about()
{
    QMessageBox::about( this, "V4L2 Control Panel",
			"This program allows easy experimenting with video4linux devices.");
}

ApplicationWindow *g_mw;

int main(int argc, char **argv)
{
    QApplication a(argc, argv);
    g_mw = new ApplicationWindow();
    g_mw->setCaption( "V4L2 Control Panel" );
    g_mw->setDevice("/dev/video0");
    g_mw->show();
    a.connect( &a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()) );
    return a.exec();
}
