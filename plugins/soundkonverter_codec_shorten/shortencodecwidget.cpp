
#include "shortencodecglobal.h"

#include "shortencodecwidget.h"
#include "../../core/conversionoptions.h"

#include <QLayout>
#include <QCheckBox>
#include <KLineEdit>
#include <KLocale>


ShortenCodecWidget::ShortenCodecWidget()
    : CodecWidget(),
    currentFormat( "shorten" )
{
    QGridLayout *grid = new QGridLayout( this );
    grid->setContentsMargins( 0, 0, 0, 0 );

    // cmd arguments box

    QHBoxLayout *cmdArgumentsBox = new QHBoxLayout();
    grid->addLayout( cmdArgumentsBox, 1, 0 );

    cCmdArguments = new QCheckBox( i18n("Additional encoder arguments:"), this );
    cmdArgumentsBox->addWidget( cCmdArguments );
    lCmdArguments = new KLineEdit( this );
    lCmdArguments->setEnabled( false );
    cmdArgumentsBox->addWidget( lCmdArguments );
    connect( cCmdArguments, SIGNAL(toggled(bool)), lCmdArguments, SLOT(setEnabled(bool)) );

    grid->setRowStretch( 2, 1 );
}

ShortenCodecWidget::~ShortenCodecWidget()
{}

ConversionOptions *ShortenCodecWidget::currentConversionOptions()
{
    ConversionOptions *options = new ConversionOptions();
    options->qualityMode = ConversionOptions::Lossless;
    if( cCmdArguments->isChecked() )
        options->cmdArguments = lCmdArguments->text();
    else
        options->cmdArguments = "";

    return options;
}

bool ShortenCodecWidget::setCurrentConversionOptions( ConversionOptions *_options )
{
    if( !_options || _options->pluginName != global_plugin_name )
        return false;

    ConversionOptions *options = _options;
    cCmdArguments->setChecked( !options->cmdArguments.isEmpty() );
    if( !options->cmdArguments.isEmpty() )
        lCmdArguments->setText( options->cmdArguments );

    return true;
}

void ShortenCodecWidget::setCurrentFormat( const QString& format )
{
    if( currentFormat == format )
        return;

    currentFormat = format;
    setEnabled( currentFormat != "wav" );
}

QString ShortenCodecWidget::currentProfile()
{
    return i18n("Lossless");
}

bool ShortenCodecWidget::setCurrentProfile( const QString& profile )
{
    return profile == i18n("Lossless");
}

int ShortenCodecWidget::currentDataRate()
{
    int dataRate;

    if( currentFormat == "wav" )
    {
        dataRate = 10590000;
    }
    else
    {
        dataRate = 6400000;
    }

    return dataRate;
}


