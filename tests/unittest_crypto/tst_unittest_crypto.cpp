#include <QTest>

#include <include/crypto_helper.h>

namespace tests {
namespace unittests {

class tst_Crypto : public QObject
{
    Q_OBJECT

  public:
  private slots:
    void checkHash();
    void checkHash_data();
};

void tst_Crypto::checkHash_data()
{
    QTest::addColumn<QString>("password");
    QTest::addColumn<QString>("salt_hex");
    QTest::addColumn<QString>("expected_hash");

    QTest::newRow("HMAC only (old algorithm)") << "password"
                                               << "73616c7473616c74"
                                               << "4128058d074264779ec23ee1ffed65d7c4e16c93003315cd5cb85170770b254a";

    QTest::newRow("PBKDF2 (new algorithm)") << "password"
                                            << "73616c7473616c7473616c7473616c74"
                                            << "4fbf2d122fe6afc61a81e9f2fe393ab39f906a78ddddc797763c0e784857e9b4";
}

void tst_Crypto::checkHash()
{
    QFETCH(QString, password);
    QFETCH(QString, salt_hex);
    QFETCH(QString, expected_hash);

    QCOMPARE(CryptoHelper::hash_password(QByteArray::fromHex(salt_hex.toUtf8()), password), expected_hash);
}

}
}

QTEST_APPLESS_MAIN(tests::unittests::tst_Crypto)

#include "tst_unittest_crypto.moc"