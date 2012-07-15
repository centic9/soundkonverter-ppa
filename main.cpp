

#include "soundkonverterapp.h"
#include "soundkonverter.h"
#include "global.h"

#include <kdeui_export.h>
#include <KMainWindow>
#include <KUniqueApplication>
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>


static const char description[] =
    I18N_NOOP("soundKonverter is a frontend to various audio converters, Replay Gain tools and CD rippers.\n\nPlease file bug reports at https://bugs.launchpad.net/soundkonverter\nor simply send me a mail to hessijames@gmail.com");

static const char version[] = SOUNDKONVERTER_VERSION_STRING;

int main(int argc, char **argv)
{
    KAboutData about("soundkonverter", 0, ki18n("soundKonverter"), version, ki18n(description), KAboutData::License_GPL, ki18n("(C) 2005-2012 Daniel Faust"), KLocalizedString(), 0, "hessijames@gmail.com");
    about.addAuthor( ki18n("Daniel Faust"), KLocalizedString(), "hessijames@gmail.com" );
    about.addCredit( ki18n("David Vignoni"), ki18n("Nuvola icon theme"), 0, "http://www.icon-king.com" );
    about.addCredit( ki18n("Scott Wheeler"), ki18n("TagLib"), "wheeler@kde.org", "http://ktown.kde.org/~wheeler" );
    about.addCredit( ki18n("Marco Nelles"), ki18n("Audex"), 0, "http://opensource.maniatek.de/audex" );
    about.addCredit( ki18n("Amarok developers"), ki18n("Amarok"), 0, "http://amarok.kde.org" );
    about.addCredit( ki18n("All programmers of audio converters"), ki18n("Backends") );
    KCmdLineArgs::init(argc, argv, &about);

    KCmdLineOptions options;
    options.add( "replaygain", ki18n("Open the Replay Gain tool an add all given files") );
    options.add( "rip <device>", ki18n("List all tracks on the cd drive <device>, 'auto' will search for a cd") );
    options.add( "profile <profile>", ki18n("Add all files using the given profile") );
    options.add( "format <format>", ki18n("Add all files using the given format") );
    options.add( "output <directory>", ki18n("Output all files to <directory>") );
    options.add( "invisible", ki18n("Start soundKonverter invisible") );
    options.add( "autostart", ki18n("Start the conversion immediately (enabled when using '--invisible')") );
    options.add( "autoclose", ki18n("Close soundKonverter after all files are converted (enabled when using '--invisible')") );
    options.add( "command <command>", ki18n("Execute <command> after each file has been converted (%i=input file, %o=output file)") );
    options.add( "+[files]", ki18n("Audio file(s) to append to the file list") );
    KCmdLineArgs::addCmdLineOptions(options);

    soundKonverterApp::addCmdLineOptions();
    if( !soundKonverterApp::start() )
    {
        return 0;
    }

    soundKonverterApp app;

    // mainWin has WDestructiveClose flag by default, so it will delete itself.
    return app.exec();
}

