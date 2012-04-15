
#ifndef NEROAACCODECWIDGET_H
#define NEROAACCODECWIDGET_H

#include "../../core/codecwidget.h"

class KComboBox;
class QSlider;
class QDoubleSpinBox;
class QCheckBox;
// class QLabel;

class NeroaacCodecWidget : public CodecWidget
{
    Q_OBJECT
public:
    NeroaacCodecWidget();
    ~NeroaacCodecWidget();

    ConversionOptions *currentConversionOptions();
    bool setCurrentConversionOptions( ConversionOptions *_options );
    void setCurrentFormat( const QString& format );
    QString currentProfile();
    bool setCurrentProfile( const QString& profile );
    QDomDocument customProfile();
    bool setCustomProfile( const QString& profile, const QDomDocument& document );
    int currentDataRate();

private:
    KComboBox *cMode;
    QSlider *sQuality;
    QDoubleSpinBox *dQuality;
    KComboBox *cBitrateMode;
//     QCheckBox *chChannels;
//     KComboBox *cChannels;
//     QCheckBox *chSamplerate;
//     KComboBox *cSamplerate;

    QString currentFormat; // holds the current output file format

    int bitrateForQuality( double quality );
    double qualityForBitrate( int bitrate );

private slots:
    void modeChanged( int mode );
    void qualitySliderChanged( int quality );
    void qualitySpinBoxChanged( double quality );
//     void channelsToggled( bool enabled );
//     void samplerateToggled( bool enabled );

signals:
    void somethingChanged();
};

#endif // NEROAACCODECWIDGET_H