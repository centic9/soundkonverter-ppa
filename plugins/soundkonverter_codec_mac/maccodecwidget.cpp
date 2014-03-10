
#include "maccodecglobal.h"

#include "maccodecwidget.h"
#include "../../core/conversionoptions.h"



#include <QLayout>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>
#include <QCheckBox>
#include <KLineEdit>
#include <KLocale>


MacCodecWidget::MacCodecWidget()
    : CodecWidget(),
    currentFormat( "ape" )
{
    QGridLayout *grid = new QGridLayout( this );
    grid->setContentsMargins( 0, 0, 0, 0 );

    // set up encoding options selection

    QHBoxLayout *topBox = new QHBoxLayout();
    grid->addLayout( topBox, 0, 0 );

    QLabel *lCompressionLevel = new QLabel( i18n("Compression level:"), this );
    topBox->addWidget( lCompressionLevel );

    sCompressionLevel = new QSlider( Qt::Horizontal, this );
    sCompressionLevel->setRange( 1, 5 );
    sCompressionLevel->setSingleStep( 1 );
    sCompressionLevel->setPageStep( 1 );
//     sQuality->setTickPosition( QSlider::TicksBelow );
//     sQuality->setFixedWidth( sQuality->sizeHint().width() );
    connect( sCompressionLevel, SIGNAL(valueChanged(int)), this, SLOT(compressionLevelSliderChanged(int)) );
    connect( sCompressionLevel, SIGNAL(valueChanged(int)), SIGNAL(optionsChanged()) );
    topBox->addWidget( sCompressionLevel );
    sCompressionLevel->setToolTip( i18n("Compression level from %1 to %2 where %2 is the best compression.\nThe better the compression, the slower the conversion but the smaller the file size and vice versa.", 1000, 5000) );

    iCompressionLevel = new QSpinBox( this );
    iCompressionLevel->setRange( 1, 5 );
    iCompressionLevel->setSingleStep( 1 );
    iCompressionLevel->setSuffix( "000" );
    iCompressionLevel->setFixedWidth( iCompressionLevel->sizeHint().width() );
//     dQuality->setFixedHeight( cMode->minimumSizeHint().height() );
    connect( iCompressionLevel, SIGNAL(valueChanged(int)), this, SLOT(compressionLevelSpinBoxChanged(int)) );
    connect( iCompressionLevel, SIGNAL(valueChanged(int)), SIGNAL(optionsChanged()) );
    topBox->addWidget( iCompressionLevel );
    iCompressionLevel->setToolTip( i18n("Compression level from %1 to %2 where %2 is the best compression.\nThe better the compression, the slower the conversion but the smaller the file size and vice versa.", 1000, 5000) );

    topBox->addStretch();

    grid->setRowStretch( 1, 1 );

    iCompressionLevel->setValue( 2 );
}

MacCodecWidget::~MacCodecWidget()
{}

ConversionOptions *MacCodecWidget::currentConversionOptions()
{
    ConversionOptions *options = new ConversionOptions();
    options->qualityMode = ConversionOptions::Lossless;
    options->compressionLevel = iCompressionLevel->value()*1000;
    return options;
}

bool MacCodecWidget::setCurrentConversionOptions( ConversionOptions *_options )
{
    if( !_options || _options->pluginName != global_plugin_name ) return false;

    ConversionOptions *options = _options;
    iCompressionLevel->setValue( options->compressionLevel/1000 );
    return true;
}

void MacCodecWidget::setCurrentFormat( const QString& format )
{
    if( currentFormat == format ) return;
    currentFormat = format;
    setEnabled( currentFormat != "wav" );
}

QString MacCodecWidget::currentProfile()
{
    return i18n("Lossless");
}

bool MacCodecWidget::setCurrentProfile( const QString& profile )
{
    return profile == i18n("Lossless");
}

int MacCodecWidget::currentDataRate()
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

void MacCodecWidget::compressionLevelSliderChanged( int quality )
{
    iCompressionLevel->setValue( quality );
}

void MacCodecWidget::compressionLevelSpinBoxChanged( int quality )
{
    sCompressionLevel->setValue( quality );
}


