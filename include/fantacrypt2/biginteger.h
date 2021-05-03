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

    BigInteger exp2();
    bool sweep();

    BigInteger operator + (BigInteger const &) const;
    BigInteger operator - (BigInteger const &) const;
    BigInteger operator * (BigInteger const &) const;
    BigInteger operator / (BigInteger const &) const;

    bool operator == (BigInteger const &) const;
    bool operator > (BigInteger const &) const;
    bool operator < (BigInteger const &) const;

    bool is_valid = false;
    bool positive;
    QList<quint8> digits;
private:

};

#endif // BIGINTEGER_H
