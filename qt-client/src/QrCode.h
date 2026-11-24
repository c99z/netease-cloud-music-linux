#pragma once

#include <QByteArray>
#include <QImage>
#include <QList>

// Minimal QR Code encoder (byte mode, versions 1-40) following ISO/IEC 18004.
// Used for the scan-to-login code, so no external QR library is needed.
class QrCode
{
public:
    enum class Ecc { Low, Medium };

    // mask < 0 picks the mask with the lowest penalty, like a standard encoder.
    static QrCode encode(const QByteArray &data, Ecc ecc = Ecc::Medium, int mask = -1);

    int size() const { return m_size; }
    int version() const { return m_version; }
    int mask() const { return m_mask; }
    bool module(int x, int y) const { return m_modules.at(y * m_size + x); }
    bool isValid() const { return m_size > 0; }
    QImage toImage(int scale = 8, int border = 4) const;

private:
    void setFunction(int x, int y, bool dark);
    void drawFunctionPatterns();
    void drawFinder(int x, int y);
    void drawAlignment(int x, int y);
    void drawFormatBits(int mask);
    void drawVersion();
    void drawCodewords(const QByteArray &codewords);
    void applyMask(int mask);
    long penalty() const;
    QList<int> alignmentPositions() const;

    int m_version = 0;
    int m_size = 0;
    int m_mask = 0;
    Ecc m_ecc = Ecc::Medium;
    QList<bool> m_modules;
    QList<bool> m_function;
};
