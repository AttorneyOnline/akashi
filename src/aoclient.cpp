#include "include/aoclient.h"

AOClient::AOClient(QHostAddress p_remote_ip)
{
  joined = false;
  password = "";
  current_area = 0;
  current_char = "";
  remote_ip = p_remote_ip;
}

QString AOClient::getHwid() { return hwid; }

void AOClient::setHwid(QString p_hwid)
{
  hwid = p_hwid;

  QCryptographicHash hash(QCryptographicHash::Md5); // Don't need security, just
                                                    // hashing for uniqueness
  QString concat_ip_id = remote_ip.toString() + p_hwid;
  hash.addData(concat_ip_id.toUtf8());

  ipid = hash.result().toHex().right(8);
}

QString AOClient::getIpid() { return ipid; }

AOClient::~AOClient() {}
