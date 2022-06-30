#include "include/network/aopacket.h"

class PacketFactory {
  public:
    // thingy here to register/map strings to constructors
    static AOPacket* createPacket(QString header, QStringList contents);
    static AOPacket* createPacket(QString raw_packet);
    template<typename T> static void registerClass(QString header) { class_map[header] = &createInstance<T>; };

  private:
    template<typename T> static AOPacket* createInstance(QStringList contents) { return new T(contents); };
    template<typename T> static AOPacket* createInstance(QString header, QStringList contents) { return new T(header, contents); };
    typedef std::map<QString, AOPacket* (*)(QStringList)> type_map;

    static inline type_map class_map;
}; 