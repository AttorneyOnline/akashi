#include "include/fantacrypt2/biginteger.h"

// Digits are stored in little endian order, in base 256

BigInteger::BigInteger(QString hex)
{
    qDebug() << "construct";
    if (hex.length() % 2 != 0)
        return;
    for (int i = 0; i < hex.length(); i += 2) {
        unsigned int digit = 0;
        QString hex_digit = hex.at(i);
        hex_digit += hex.at(i + 1);

        bool can_convert = false;
        digit = hex_digit.toUInt(&can_convert, 16);
        if (!can_convert)
            return;

        digits.push_front(digit & 0xFF);
    }

    is_valid = true;
}

BigInteger::BigInteger()
{
    is_valid = true;
}

QString BigInteger::toString() {
    QString result;

    for (int i = digits.size() - 1; i >= 0; i--) {
        result += QString::number(digits.at(i), 16);
    }

    return result;
}

BigInteger BigInteger::operator + (BigInteger const &a) const
{
    BigInteger b = *this;
    BigInteger result;
    int n = a.digits.size();
    if (n < b.digits.size())
        n = b.digits.size();

    int carry = 0;
    for (int i = 0; i < n; i++) {
        result.digits.push_back(0);
        int addition_result = carry;

        if (i < a.digits.size() && i < b.digits.size())
            addition_result += a.digits.at(i) + b.digits.at(i);
        else if (i >= a.digits.size())
            addition_result += b.digits.at(i);
        else if (i >= b.digits.size())
            addition_result += a.digits.at(i);

        result.digits[i] = addition_result & 0x00FF;
        carry = (addition_result & 0xFF00) >> 8;
    }

    result.is_valid = true;
    return result;
}

BigInteger BigInteger::operator * (BigInteger const &a) const
{
    BigInteger b = *this;
    BigInteger result;

    BigInteger zero("00");

    while (a > zero) {
        result = result + b;
    }

    return result;
}
