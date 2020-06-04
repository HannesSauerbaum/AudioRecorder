#include "AutomaticGainControl.h"

#define AGCMAX 63
#define AGCMIN 40

AutomaticGainControl::AutomaticGainControl(int AGChangtime, double AGCtresh, double AGCslopeIncrease, double AGCslopeDecrease, double peak)
    : m_gainReduceChecker(),
    m_gainReduceCounter(),
    m_agcAttack(1)    
{
    m_agcHangtime = AGChangtime;
    m_agcThresh = AGCtresh;
    m_agcSlopeIncrease = AGCslopeIncrease;
    m_agcSlopeDecrease = AGCslopeDecrease;
    m_peak = peak;
}

double AutomaticGainControl::AGC(double AGCvalue)
{
    static int m_hangtimer;
    static double m_agcGain = 40;
    static double m_agcGainOut;

    // Check if signal is too loud
    for (m_gainReduceCounter = 0, m_gainReduceChecker = 0; m_gainReduceCounter < m_agcAttack; m_gainReduceCounter++)
    {
        if (m_peak > m_agcThresh) m_gainReduceChecker++;
    }
   
    // Gain Decrease
    if ((m_gainReduceChecker == m_agcAttack) || ((m_gainReduceChecker > 0) && (m_hangtimer >= m_agcHangtime)))
    {
        m_agcGainOut = (m_agcGain - m_agcSlopeDecrease);

        // Reduce limit 
        if (m_agcGainOut <= AGCMIN) m_agcGainOut = AGCMIN;

        m_agcGain = m_agcGainOut;

        // Reset hangtimer
        m_hangtimer = 0;
    }

    // hangtimer before increasing
    else if (m_peak < m_agcThresh)
    {
        m_hangtimer++;
    }

    // Gain Increase
    if (m_hangtimer >= m_agcHangtime)
    {
        m_agcGainOut = m_agcGain + m_agcSlopeIncrease;

        // Gain limit
        if (m_agcGainOut >= AGCMAX) m_agcGainOut = AGCMAX;

        m_agcGain = m_agcGainOut;
    }
    return m_agcGainOut;
}

