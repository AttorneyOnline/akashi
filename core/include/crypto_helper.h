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
    static constexpr int argon2_salt_len = 16;
    static constexpr int argon2_cost = 10000;
    static constexpr int argon2_output_len = 16;

    static QString argon2(QString salt, QString password) {
      return "UNIMPLEMENTED";
    }

    static QString password_hmac(QString salt, QString password) {
      QMessageAuthenticationCode hmac(QCryptographicHash::Sha256);
      hmac.setKey(salt.toUtf8());
      hmac.addData(password.toUtf8());
      return hmac.result().toHex();
    }
  public:

    static QString hash_password(QString salt, QString password) {
      // Select the correct hash backend based on the salt length
      if (salt.length() < argon2_salt_len)
        return password_hmac(salt, password);

      return argon2(salt, password);

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