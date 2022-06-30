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
#ifndef CRYPTO_HELPER_H
#define CRYPTO_HELPER_H

#include <QString>
#include <QMessageAuthenticationCode>
#if QT_VERSION > QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#endif

/**
 * @brief Simple header library for basic cryptographic functionality
 * 
 */
class CryptoHelper {
  private:
    static constexpr qint32 pbkdf2_output_len = 32; // 32 bytes (SHA-256)
    static constexpr quint32 pbkdf2_cost = 100000;

    static QByteArray hmac(QByteArray salt, QByteArray password) {
      QMessageAuthenticationCode hmac(QCryptographicHash::Sha256);
      hmac.setKey(salt);
      hmac.addData(password);
      return hmac.result();
    }

    static QString password_hmac(QString salt, QString password) {
      return hmac(salt.toUtf8(), password.toUtf8()).toHex();
    }

    static QString pbkdf2(QByteArray salt, QString password) {
      QByteArray bigendian_one("\x00\x00\x00\x01", 4);
      QByteArray last_block = salt;
      last_block.append(bigendian_one);

      QByteArray result(pbkdf2_output_len, '\0');

      for (unsigned int i = 0; i < pbkdf2_cost; i++) {
        last_block = hmac(password.toUtf8(), last_block);
        for (unsigned int n = 0; n < pbkdf2_output_len; n++)
          result[n] = result[n] ^ last_block[n];
      }

      return result.toHex();
    }
  public:
    static constexpr qint32 pbkdf2_salt_len = 16;

    static QString hash_password(QByteArray salt, QString password) {
      // Select the correct hash backend based on the salt length
      if (salt.length() < pbkdf2_salt_len) {
        // Due to an implementation oversight, the old HMAC algorithm
        // does not correctly handle salts. Instead of treating the hex string
        // as binary data, it is used directly. We have to handle this case.
        return password_hmac(salt.toHex(), password);
      }
        
      return pbkdf2(salt, password);

    }

    static quint8 rand8() {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
      qsrand(QDateTime::currentMSecsSinceEpoch());
      quint32 l_rand = qrand();
#else
      quint32 l_rand = QRandomGenerator::system()->generate();
#endif
      return (quint8) (l_rand & 0xFF);
    }

    static quint64 rand64() {
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
      qsrand(QDateTime::currentMSecsSinceEpoch());
      quint32 l_upper = qrand();
      quint32 l_lower = qrand();
      quint64 l_number = (l_upper << 32) | l_lower;
#else
      quint64 l_number = QRandomGenerator::system()->generate64();
#endif
      return l_number;
    }

    static QByteArray randbytes(int n) {
      QByteArray output;
      for (int i = 0; i < n; i++) {
        output.append(CryptoHelper::rand8());
      }
      return output;
    }
};

#endif