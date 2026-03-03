// gratten_limits.h
#pragma once

#include <QString>
#include <QVector>

struct GrattenCommandInfo {
    QString key;
    QString setPattern;
    QString queryCmd;
    double minVal;
    double maxVal;
    QString unit;
    bool isBoolean;
};

inline const QVector<GrattenCommandInfo> GRATTEN_COMMANDS = {
    // Частота несущей (значение в Гц, но с суффиксом Hz)
    {"FREQ",     ":FREQuency:CW %1Hz",       ":FREQuency:CW?",   250e3, 4e9,   "Hz",   false},
    // Амплитуда (уже есть DBM)
    {"AMPL",     ":POWer:LEVEL %1DBM",       ":POWer:LEVEL?",   -136.0, 13.0,  "dBm",  false},
    // Выход RF (булев)
    {"OUTP",     ":OUTPut:STATE %1",         ":OUTPut:STATE?",   0,     1,     "",     true},
    // AM частота
    {"AMFREQ",   ":AM:INTernal:FREQuency %1Hz", ":AM:INTernal:FREQuency?", 20, 20000, "Hz", false},
    // AM глубина (с процентом)
    {"AMDEPTH",  ":AM:DEPTh %1%",            ":AM:DEPTh?",       0,     100,   "%",    false},
    // AM состояние (булев)
    {"AMSTATE",  ":AM:STATE %1",             ":AM:STATE?",       0,     1,     "",     true}
};
