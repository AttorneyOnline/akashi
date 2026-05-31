//////////////////////////////////////////////////////////////////////////////////////
//    akashi - a server for Attorney Online 2                                       //
//    Copyright (C) 2022  scatterflower                                             //
//                                                                                  //
//    This program is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU Affero General Public License as                //
//    published by the Free Software Foundation, either version 3 of the            //
//    License, or (at your option) any later version.                               //
//                                                                                  //
//    This program is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of                //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 //
//    GNU Affero General Public License for more details.                           //
//                                                                                  //
//    You should have received a copy of the GNU Affero General Public License      //
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.        //
//////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <QMessageAuthenticationCode>
#include <QPasswordDigestor>
#include <QString>
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

/**
 * @brief Simple header library for basic cryptographic functionality
 */
class CryptoHelper
{
  private:
    /**
     * @brief Length of the output of PBKDF2
     */
    static constexpr qint32 pbkdf2_output_len = 32; // 32 bytes (SHA-256)
    /**
     * @brief Configurable cost parameter of PBKDF2
     */
    static constexpr quint32 pbkdf2_cost = 100000;

    /**
     * @brief Compute the SHA-256 HMAC of given data
     *
     * @param salt HMAC key
     * @param password HMAC data
     * @return QByteArray HMAC result
     */
    static QByteArray hmac(QByteArray salt, QByteArray password)
    {
        QMessageAuthenticationCode hmac(QCryptographicHash::Sha256);
        hmac.setKey(salt);
        hmac.addData(password);
        return hmac.result();
    }

    /**
     * @brief Compute the SHA-256 HMAC of strings
     *
     * @param salt Salt value
     * @param password Password value
     * @return QString HMAC result, hex encoded
     */
    static QString password_hmac(QString salt, QString password)
    {
        return hmac(salt.toUtf8(), password.toUtf8()).toHex();
    }

    /**
     * @brief Perform the PBKDF2 key-derivation function
     *
     * @details PBKDF2 is a very configurable algorithm, but most of its functionality
     * does not apply here. Instead, we fix the output to the size of the underlying
     * hash function, which greatly simplifies the algorithm.
     *
     * @param salt Salt value
     * @param password Password value
     * @return QString PBKDF2 result, hex encoded
     */
    static QString pbkdf2(QByteArray salt, QString password)
    {
        // Qt's vetted PBKDF2-HMAC-SHA256, same parameters as the old hand-rolled loop.
        return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256, password.toUtf8(), salt, pbkdf2_cost, pbkdf2_output_len).toHex();
    }

  public:
    /**
     * @brief The length of the salt value used for PBKDF2, in bytes
     */
    static constexpr qint32 pbkdf2_salt_len = 16;

    /**
     * @brief Compute the hash of a password to store, given a salt
     *
     * @details This function selects one of two hashing algorithm backends to use,
     * dependent on the length of the salt. Older versions of this program used an
     * 8-byte salt, but newer versions use a 16-byte salt. This allows us to version
     * the algorithm being used without the need to change the schema of the database.
     *
     * @param salt Salt value
     * @param password Password value
     * @return QString Hashed password, hex encoded
     */
    static QString hash_password(QByteArray salt, QString password)
    {
        // Select the correct hash backend based on the salt length
        if (salt.length() < pbkdf2_salt_len) {
            // Due to an implementation oversight, the old HMAC algorithm
            // does not correctly handle salts. Instead of treating the hex string
            // as binary data, it is used directly. We have to handle this case.
            return password_hmac(salt.toHex(), password);
        }

        return pbkdf2(salt, password);
    }

    /**
     * @brief Compares two hex-encoded hashes without leaking their contents through timing.
     *
     * @param f_left First hash.
     * @param f_right Second hash.
     * @return True if the hashes are equal.
     */
    static bool constantTimeEquals(const QString &f_left, const QString &f_right)
    {
        const QByteArray l_left = f_left.toUtf8();
        const QByteArray l_right = f_right.toUtf8();
        // The hash length is fixed and public, so comparing it up front leaks nothing.
        if (l_left.size() != l_right.size()) {
            return false;
        }
        quint8 l_difference = 0;
        for (int i = 0; i < l_left.size(); i++) {
            l_difference |= quint8(l_left[i]) ^ quint8(l_right[i]);
        }
        return l_difference == 0;
    }

    /**
     * @brief Generate a random octet
     *
     * @return quint8 A random 8-bit value
     */
    static quint8 rand8()
    {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
        qsrand(QDateTime::currentMSecsSinceEpoch());
        quint32 l_rand = qrand();
#else
        quint32 l_rand = QRandomGenerator::system()->generate();
#endif
        return (quint8)(l_rand & 0xFF);
    }

    /**
     * @brief Generate an arbitrary amount of random data
     *
     * @param n Number of bytes to generate
     * @return QByteArray Random bytes
     */
    static QByteArray randbytes(int n)
    {
        QByteArray output;
        for (int i = 0; i < n; i++) {
            output.append(CryptoHelper::rand8());
        }
        return output;
    }
};

