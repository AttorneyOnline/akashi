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
    BigInteger result;

    const int aSize = a.digits.size();
    const int bSize = this->digits.size();

    const int n = std::max(aSize, bSize);

    result.digits.reserve(n);

    int carry = 0;
    int addition_result = 0;
    for (int i = 0; i < n; i++) {
        addition_result = carry;

        if (i < aSize)
            addition_result += a.digits.at(i);
        if (i < bSize)
            addition_result += this->digits.at(i);

        result.digits.append(addition_result & 0x00FF);
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
    BigInteger result;
    for (int i = 0; i < digits.size() + a.digits.size(); i++)
        result.digits.append(0);

    for (int i = 0; i < digits.length(); i++) {
        int carry = 0;
        int digit;

        for (int j = i; j < a.digits.length() + i; j++) {
            digit = result.digits.at(j) + (digits.at(i) * a.digits.at(j - i)) + carry;
            carry = (digit & 0xFF00) >> 8;
            result.digits[j] = digit & 0xFF;
        }
        if (carry) {
            int j = (a.digits.length() + i);
            digit = result.digits.at(j) + carry;
            carry = (digit & 0xFF00) >> 8;
            result.digits[j] = digit & 0xFF;
        }
    }

    for (int i = result.digits.length() - 1; i >= 0; i--) {
        if (result.digits.at(i) == 0)
            result.digits.pop_back();
    }

    return result;
}

// returns 2^this
BigInteger BigInteger::exp2()
{
    BigInteger result;



    return BigInteger();
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

    if (a == *this)
        return false;

    if (this->digits.size() > a.digits.size())
        return true;

    if (a.digits.size() > this->digits.size())
        return false;

    for (int i = a.digits.size() - 1; i > 0; i--) {
        if (this->digits.at(i) > a.digits.at(i))
            return true;
        if (this->digits.at(i) < a.digits.at(i))
            return false;
    }

    return false;
}

bool BigInteger::operator < (const BigInteger &a) const
{
    if (a == *this)
        return false;

    if (*this > a)
        return false;

    return true;
}
