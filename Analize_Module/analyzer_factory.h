#ifndef ANALYZER_FACTORY_H
#define ANALYZER_FACTORY_H

#pragma once

#include "packetanalyzer_adapter.h"

class AnalyzerFactory {
public:
    static PacketAnalyzerInterface* createAnalyzer(QObject* parent = nullptr) {
        return new PacketAnalyzerAdapter(parent);
    }
};

#endif // ANALYZER_FACTORY_H
