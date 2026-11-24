#include "NeteaseCrypto.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QUrl>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <memory>

namespace {
const QByteArray presetKey = "0CoJUm6Qyw8W8jud";
const QByteArray cbcIv = "0102030405060708";
const QByteArray eapiKey = "e82ckenh8dichen8";
// Public key of the web client (1024-bit modulus, exponent 65537).
const char *rsaModulus =
    "e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b725152b3ab17a876aea8a5aa76d2e417629ec4ee341f56135fccf695280104e0312ecbda92557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424d813cfe4875d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7";
const QByteArray base62 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

QByteArray formValue(const QByteArray &value)
{
    return QUrl::toPercentEncoding(QString::fromLatin1(value));
}
}

namespace NeteaseCrypto {

QByteArray aes128(const QByteArray &plain, const QByteArray &key, const QByteArray &iv, bool cbc)
{
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    EVP_EncryptInit_ex(ctx.get(), cbc ? EVP_aes_128_cbc() : EVP_aes_128_ecb(), nullptr,
                       reinterpret_cast<const unsigned char *>(key.constData()),
                       cbc ? reinterpret_cast<const unsigned char *>(iv.constData()) : nullptr);
    QByteArray out(plain.size() + 16, Qt::Uninitialized);
    int length = 0;
    int tail = 0;
    EVP_EncryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &length,
                      reinterpret_cast<const unsigned char *>(plain.constData()), int(plain.size()));
    EVP_EncryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data()) + length, &tail);
    out.resize(length + tail);
    return out;
}

QByteArray aes128Decrypt(const QByteArray &cipher, const QByteArray &key)
{
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_ecb(), nullptr,
                       reinterpret_cast<const unsigned char *>(key.constData()), nullptr);
    QByteArray out(cipher.size() + 16, Qt::Uninitialized);
    int length = 0;
    int tail = 0;
    EVP_DecryptUpdate(ctx.get(), reinterpret_cast<unsigned char *>(out.data()), &length,
                      reinterpret_cast<const unsigned char *>(cipher.constData()), int(cipher.size()));
    if (!EVP_DecryptFinal_ex(ctx.get(), reinterpret_cast<unsigned char *>(out.data()) + length, &tail))
        return {};
    out.resize(length + tail);
    return out;
}

QByteArray rsaNoPadding(const QByteArray &plain)
{
    // Textbook RSA (m^e mod n) with the message as a big-endian integer, as node-forge
    // does with 'NONE' padding; the result is left-padded to 256 hex digits.
    std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)> ctx(BN_CTX_new(), BN_CTX_free);
    BIGNUM *n = nullptr;
    BN_hex2bn(&n, rsaModulus);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> modulus(n, BN_free);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> exponent(BN_new(), BN_free);
    BN_set_word(exponent.get(), 65537);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> message(
        BN_bin2bn(reinterpret_cast<const unsigned char *>(plain.constData()), int(plain.size()), nullptr), BN_free);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> result(BN_new(), BN_free);
    BN_mod_exp(result.get(), message.get(), exponent.get(), modulus.get(), ctx.get());
    char *hex = BN_bn2hex(result.get());
    QByteArray text = QByteArray(hex).toLower();
    OPENSSL_free(hex);
    return text.rightJustified(256, '0');
}

QByteArray weapi(const QByteArray &json)
{
    QByteArray secret(16, Qt::Uninitialized);
    for (auto &c : secret)
        c = base62.at(QRandomGenerator::global()->bounded(62));
    const auto first = aes128(json, presetKey, cbcIv, true).toBase64();
    const auto params = aes128(first, secret, cbcIv, true).toBase64();
    QByteArray reversed(secret.size(), Qt::Uninitialized);
    std::reverse_copy(secret.cbegin(), secret.cend(), reversed.begin());
    return "params=" + formValue(params) + "&encSecKey=" + rsaNoPadding(reversed);
}

QByteArray eapi(const QString &path, const QByteArray &json)
{
    const QByteArray url = path.toUtf8();
    const QByteArray digest = QCryptographicHash::hash("nobody" + url + "use" + json + "md5forencrypt",
                                                       QCryptographicHash::Md5).toHex();
    const QByteArray data = url + "-36cd479b6b5-" + json + "-36cd479b6b5-" + digest;
    return "params=" + aes128(data, eapiKey, {}, false).toHex().toUpper();
}

QByteArray eapiDecrypt(const QByteArray &cipher)
{
    return aes128Decrypt(cipher, eapiKey);
}
}
