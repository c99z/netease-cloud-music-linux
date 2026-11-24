#pragma once

#include <QByteArray>
#include <QString>

// Request encodings used by music.163.com, implemented with OpenSSL.
namespace NeteaseCrypto {
// Browser (weapi): AES-128-CBC twice (preset key, then a random key) and the random key
// RSA-encrypted without padding. Returns the form body "params=...&encSecKey=...".
QByteArray weapi(const QByteArray &json);
// Desktop client (eapi): AES-128-ECB over "path-36cd479b6b5-json-36cd479b6b5-md5".
// Returns the form body "params=<HEX>".
QByteArray eapi(const QString &path, const QByteArray &json);
// Decrypts an eapi response body (AES-128-ECB, used when e_r is requested).
QByteArray eapiDecrypt(const QByteArray &cipher);

QByteArray aes128(const QByteArray &plain, const QByteArray &key, const QByteArray &iv, bool cbc);
QByteArray aes128Decrypt(const QByteArray &cipher, const QByteArray &key);
QByteArray rsaNoPadding(const QByteArray &plain);
}
