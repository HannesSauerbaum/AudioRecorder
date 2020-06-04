#pragma once
class AutomaticGainControl
{
public:
    AutomaticGainControl(int AGChangtime, double AGCtresh, double AGCslopeIncrease, double AGCslopeDecrease, double peak);
    double AGC(double AGCvalue);

protected:
    int m_agcHangtime;
    double m_agcThresh;
    double m_agcSlopeIncrease;
    double m_agcSlopeDecrease;
    double m_peak;

    int m_gainReduceChecker;
    int m_gainReduceCounter;
    int m_agcAttack;
};

