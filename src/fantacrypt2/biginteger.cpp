#include "include/fantacrypt2/biginteger.h"

// Digits are stored in little endian order, in base 256

BigInteger::BigInteger(QString hex)
{
    if (hex.at(0) == "-") {
        positive = false;
        hex = hex.right(hex.size() - 1);
    }
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
    positive = true;
    is_valid = true;
}

QString BigInteger::toString() {
    QString result;

    for (int i = digits.size() - 1; i >= 0; i--) {
        QString conv = QString::number(digits.at(i), 16);
        if (conv.length() == 1)
            conv = "0" + conv;
        result += conv;
    }

    if (!positive)
        result = "-" + result;

    return result;
}

BigInteger BigInteger::operator + (BigInteger const &a) const
{
    BigInteger b = *this;
    BigInteger result;

    if (!b.positive) {
        b.positive = true;
        return a - b;
    }

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

    if (carry != 0)
        result.digits.push_back(carry);

    result.is_valid = true;
    return result;
}

BigInteger BigInteger::operator - (BigInteger const &a) const
{
    BigInteger b = *this;
    BigInteger result;

    if (!b.positive) {
        b.positive = true;
        return a + b;
    }

    int n = a.digits.size();
    if (n < b.digits.size())
        n = b.digits.size();

    int borrow = 0;
    for (int i = 0; i < n; i++) {
        result.digits.push_back(0);
        int subtraction_result = 0;

        if (i < a.digits.size() && i < b.digits.size())
            subtraction_result -= a.digits.at(i) - b.digits.at(i);
        else if (i >= a.digits.size())
            subtraction_result = b.digits.at(i);
        else if (i >= b.digits.size())
            subtraction_result = a.digits.at(i);

        subtraction_result -= borrow;

        if (subtraction_result >= 0) {
            result.digits[i] = subtraction_result;
            borrow = 0;
        }
        else {
            result.digits[i] = (0x100 + subtraction_result);
            borrow = 1;
        }
    }

    return result;
}

BigInteger BigInteger::operator * (BigInteger const &a) const
{
    BigInteger b = *this;
    BigInteger result("00");

    BigInteger one("01");

    for (BigInteger i("00"); i < b; i = i + one) {
        result = result + a;
    }

    return result;
}

bool BigInteger::operator == (const BigInteger &a) const
{
    BigInteger b = *this;

    if (a.digits.size() != b.digits.size())
        return false;

    for (int i = 0; i < a.digits.size(); i++) {
        if (a.digits.at(i) != b.digits.at(i))
            return false;
    }

    return true;
}

bool BigInteger::operator > (const BigInteger &a) const
{
    BigInteger b = *this;

    if (a == b)
        return false;

    if (b.digits.size() > a.digits.size())
        return true;

    if (a.digits.size() > b.digits.size())
        return false;

    for (int i = a.digits.size() - 1; i > 0; i--) {
        if (b.digits.at(i) > a.digits.at(i))
            return true;
        if (b.digits.at(i) < a.digits.at(i))
            return false;
    }

    return false;
}

bool BigInteger::operator < (const BigInteger &a) const
{
    BigInteger b = *this;

    if (a == b)
        return false;

    if (b > a)
        return false;

    return true;
}
