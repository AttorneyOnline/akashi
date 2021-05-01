#ifndef BIGINTEGER_H
#define BIGINTEGER_H

#include <QList>
#include <QDebug>

class BigInteger
{
public:
    BigInteger(QString hex);
    BigInteger();

    QString toString();

    BigInteger operator + (BigInteger const &) const;
    BigInteger operator * (BigInteger const &) const;

    bool is_valid = false;
    QList<unsigned char> digits;
private:

};

#endif // BIGINTEGER_H
