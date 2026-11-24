#include "QrCode.h"

#include <QColor>
#include <algorithm>
#include <climits>

namespace {
// Indexed by [ecc][version]; ecc 0 = Low, 1 = Medium.
constexpr int eccPerBlock[2][41] = {
    {-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
};
constexpr int blockCount[2][41] = {
    {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8, 8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
};

int rawDataModules(int version)
{
    int result = (16 * version + 128) * version + 64;
    if (version >= 2) {
        const int align = version / 7 + 2;
        result -= (25 * align - 10) * align - 55;
        if (version >= 7)
            result -= 36;
    }
    return result;
}

int dataCodewords(int version, int ecc)
{
    return rawDataModules(version) / 8 - eccPerBlock[ecc][version] * blockCount[ecc][version];
}

quint8 gfMultiply(quint8 x, quint8 y)
{
    int z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return quint8(z);
}

QByteArray rsDivisor(int degree)
{
    QByteArray result(degree, 0);
    result[degree - 1] = 1;
    quint8 root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[j] = char(gfMultiply(quint8(result[j]), root));
            if (j + 1 < degree)
                result[j] = char(quint8(result[j]) ^ quint8(result[j + 1]));
        }
        root = gfMultiply(root, 0x02);
    }
    return result;
}

QByteArray rsRemainder(const QByteArray &data, const QByteArray &divisor)
{
    QByteArray result(divisor.size(), 0);
    for (const char byte : data) {
        const quint8 factor = quint8(byte) ^ quint8(result[0]);
        result.remove(0, 1);
        result.append(char(0));
        for (int i = 0; i < result.size(); ++i)
            result[i] = char(quint8(result[i]) ^ gfMultiply(quint8(divisor[i]), factor));
    }
    return result;
}

bool bit(long value, int index)
{
    return (value >> index) & 1;
}
}

QrCode QrCode::encode(const QByteArray &data, Ecc ecc, int mask)
{
    const int level = ecc == Ecc::Low ? 0 : 1;
    int version = 1;
    for (; version <= 40; ++version) {
        const int countBits = version <= 9 ? 8 : 16;
        if (4 + countBits + data.size() * 8 <= dataCodewords(version, level) * 8)
            break;
    }
    if (version > 40)
        return {};

    // Bit stream: byte mode, length, payload, terminator and padding.
    QList<bool> bits;
    auto append = [&bits](long value, int length) {
        for (int i = length - 1; i >= 0; --i)
            bits.append(bit(value, i));
    };
    append(0x4, 4);
    append(data.size(), version <= 9 ? 8 : 16);
    for (const char byte : data)
        append(quint8(byte), 8);
    const int capacity = dataCodewords(version, level) * 8;
    append(0, std::min(4, int(capacity - bits.size())));
    append(0, int((8 - bits.size() % 8) % 8));
    for (quint8 pad = 0xEC; bits.size() < capacity; pad ^= 0xEC ^ 0x11)
        append(pad, 8);
    QByteArray codewords(bits.size() / 8, 0);
    for (int i = 0; i < bits.size(); ++i)
        codewords[i >> 3] = char(quint8(codewords[i >> 3]) | (bits[i] << (7 - (i & 7))));

    // Split into blocks, add error correction, and interleave.
    const int blocks = blockCount[level][version];
    const int eccLength = eccPerBlock[level][version];
    const int rawCodewords = rawDataModules(version) / 8;
    const int shortBlocks = blocks - rawCodewords % blocks;
    const int shortLength = rawCodewords / blocks;
    const auto divisor = rsDivisor(eccLength);
    QList<QByteArray> parts;
    for (int i = 0, offset = 0; i < blocks; ++i) {
        const int length = shortLength - eccLength + (i < shortBlocks ? 0 : 1);
        QByteArray block = codewords.mid(offset, length);
        offset += length;
        const auto remainder = rsRemainder(block, divisor);
        if (i < shortBlocks)
            block.append(char(0));
        parts.append(block + remainder);
    }
    QByteArray interleaved;
    for (int i = 0; i < parts.first().size(); ++i) {
        for (int j = 0; j < parts.size(); ++j) {
            if (i != shortLength - eccLength || j >= shortBlocks)
                interleaved.append(parts[j][i]);
        }
    }

    QrCode qr;
    qr.m_version = version;
    qr.m_size = version * 4 + 17;
    qr.m_ecc = ecc;
    qr.m_modules = QList<bool>(qr.m_size * qr.m_size, false);
    qr.m_function = QList<bool>(qr.m_size * qr.m_size, false);
    qr.drawFunctionPatterns();
    qr.drawCodewords(interleaved);

    if (mask < 0) {
        long best = LONG_MAX;
        for (int candidate = 0; candidate < 8; ++candidate) {
            qr.applyMask(candidate);
            qr.drawFormatBits(candidate);
            const long score = qr.penalty();
            if (score < best) {
                best = score;
                mask = candidate;
            }
            qr.applyMask(candidate); // undo (masking is an XOR)
        }
    }
    qr.m_mask = mask;
    qr.applyMask(mask);
    qr.drawFormatBits(mask);
    return qr;
}

void QrCode::setFunction(int x, int y, bool dark)
{
    m_modules[y * m_size + x] = dark;
    m_function[y * m_size + x] = true;
}

QList<int> QrCode::alignmentPositions() const
{
    if (m_version == 1)
        return {};
    const int count = m_version / 7 + 2;
    const int step = m_version == 32 ? 26 : (m_version * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
    QList<int> result{6};
    for (int position = m_size - 7; result.size() < count; position -= step)
        result.insert(1, position);
    return result;
}

void QrCode::drawFunctionPatterns()
{
    for (int i = 0; i < m_size; ++i) {
        setFunction(6, i, i % 2 == 0);
        setFunction(i, 6, i % 2 == 0);
    }
    drawFinder(3, 3);
    drawFinder(m_size - 4, 3);
    drawFinder(3, m_size - 4);
    const auto positions = alignmentPositions();
    const int last = int(positions.size()) - 1;
    for (int i = 0; i <= last; ++i) {
        for (int j = 0; j <= last; ++j) {
            if ((i == 0 && j == 0) || (i == 0 && j == last) || (i == last && j == 0))
                continue;
            drawAlignment(positions[i], positions[j]);
        }
    }
    drawFormatBits(0); // reserves the area; real bits are written after masking
    drawVersion();
}

void QrCode::drawFinder(int x, int y)
{
    for (int dy = -4; dy <= 4; ++dy) {
        for (int dx = -4; dx <= 4; ++dx) {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            const int xx = x + dx;
            const int yy = y + dy;
            if (xx >= 0 && xx < m_size && yy >= 0 && yy < m_size)
                setFunction(xx, yy, distance != 2 && distance != 4);
        }
    }
}

void QrCode::drawAlignment(int x, int y)
{
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx)
            setFunction(x + dx, y + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }
}

void QrCode::drawFormatBits(int mask)
{
    const int eccBits = m_ecc == Ecc::Low ? 1 : 0;
    const int data = eccBits << 3 | mask;
    int remainder = data;
    for (int i = 0; i < 10; ++i)
        remainder = (remainder << 1) ^ ((remainder >> 9) * 0x537);
    const int bits = (data << 10 | remainder) ^ 0x5412;
    for (int i = 0; i <= 5; ++i)
        setFunction(8, i, bit(bits, i));
    setFunction(8, 7, bit(bits, 6));
    setFunction(8, 8, bit(bits, 7));
    setFunction(7, 8, bit(bits, 8));
    for (int i = 9; i < 15; ++i)
        setFunction(14 - i, 8, bit(bits, i));
    for (int i = 0; i < 8; ++i)
        setFunction(m_size - 1 - i, 8, bit(bits, i));
    for (int i = 8; i < 15; ++i)
        setFunction(8, m_size - 15 + i, bit(bits, i));
    setFunction(8, m_size - 8, true);
}

void QrCode::drawVersion()
{
    if (m_version < 7)
        return;
    int remainder = m_version;
    for (int i = 0; i < 12; ++i)
        remainder = (remainder << 1) ^ ((remainder >> 11) * 0x1F25);
    const long bits = long(m_version) << 12 | remainder;
    for (int i = 0; i < 18; ++i) {
        const bool dark = bit(bits, i);
        const int a = m_size - 11 + i % 3;
        const int b = i / 3;
        setFunction(a, b, dark);
        setFunction(b, a, dark);
    }
}

void QrCode::drawCodewords(const QByteArray &codewords)
{
    int i = 0;
    for (int right = m_size - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;
        for (int vertical = 0; vertical < m_size; ++vertical) {
            for (int j = 0; j < 2; ++j) {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? m_size - 1 - vertical : vertical;
                if (!m_function[y * m_size + x] && i < codewords.size() * 8) {
                    m_modules[y * m_size + x] = bit(quint8(codewords[i >> 3]), 7 - (i & 7));
                    ++i;
                }
            }
        }
    }
}

void QrCode::applyMask(int mask)
{
    for (int y = 0; y < m_size; ++y) {
        for (int x = 0; x < m_size; ++x) {
            bool invert = false;
            switch (mask) {
            case 0: invert = (x + y) % 2 == 0; break;
            case 1: invert = y % 2 == 0; break;
            case 2: invert = x % 3 == 0; break;
            case 3: invert = (x + y) % 3 == 0; break;
            case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
            case 5: invert = x * y % 2 + x * y % 3 == 0; break;
            case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
            case 7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            }
            const int index = y * m_size + x;
            if (!m_function[index] && invert)
                m_modules[index] = !m_modules[index];
        }
    }
}

long QrCode::penalty() const
{
    long result = 0;
    // Runs of five or more same-colored modules in rows and columns.
    for (int pass = 0; pass < 2; ++pass) {
        for (int a = 0; a < m_size; ++a) {
            int run = 1;
            for (int b = 1; b < m_size; ++b) {
                const bool current = pass ? module(a, b) : module(b, a);
                const bool previous = pass ? module(a, b - 1) : module(b - 1, a);
                if (current == previous) {
                    ++run;
                } else {
                    if (run >= 5)
                        result += run - 2;
                    run = 1;
                }
            }
            if (run >= 5)
                result += run - 2;
        }
    }
    // 2x2 blocks of one color.
    for (int y = 0; y + 1 < m_size; ++y) {
        for (int x = 0; x + 1 < m_size; ++x) {
            const bool c = module(x, y);
            if (c == module(x + 1, y) && c == module(x, y + 1) && c == module(x + 1, y + 1))
                result += 3;
        }
    }
    // Finder-like 1:1:3:1:1 patterns.
    auto finderLike = [](const QList<bool> &line, int i) {
        static const bool pattern[] = {true, false, true, true, true, false, true};
        for (int k = 0; k < 7; ++k) {
            if (line[i + k] != pattern[k])
                return false;
        }
        return true;
    };
    for (int pass = 0; pass < 2; ++pass) {
        for (int a = 0; a < m_size; ++a) {
            QList<bool> line(m_size + 8, false);
            for (int b = 0; b < m_size; ++b)
                line[b + 4] = pass ? module(a, b) : module(b, a);
            for (int i = 0; i + 7 <= line.size(); ++i) {
                if (!finderLike(line, i))
                    continue;
                bool before = i >= 4, after = i + 11 <= line.size();
                for (int k = 1; before && k <= 4; ++k)
                    before = !line[i - k];
                for (int k = 7; after && k < 11; ++k)
                    after = !line[i + k];
                if (before || after)
                    result += 40;
            }
        }
    }
    // Balance of dark and light modules.
    const int dark = int(std::count(m_modules.cbegin(), m_modules.cend(), true));
    const int total = m_size * m_size;
    result += long((std::abs(dark * 20 - total * 10) + total - 1) / total - 1) * 10;
    return result;
}

QImage QrCode::toImage(int scale, int border) const
{
    const int edge = (m_size + border * 2) * scale;
    QImage image(edge, edge, QImage::Format_RGB32);
    image.fill(Qt::white);
    for (int y = 0; y < m_size; ++y) {
        for (int x = 0; x < m_size; ++x) {
            if (!module(x, y))
                continue;
            for (int dy = 0; dy < scale; ++dy) {
                auto *line = reinterpret_cast<QRgb *>(image.scanLine((y + border) * scale + dy));
                std::fill(line + (x + border) * scale, line + (x + border + 1) * scale, qRgb(0, 0, 0));
            }
        }
    }
    return image;
}
