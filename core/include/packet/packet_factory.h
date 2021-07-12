#include "aopacket.h"

class PacketFactory {
  public:
    // thingy here to register/map strings to constructors
    static AOPacket* createPacket(QString header, QStringList contents);
    static AOPacket* createPacket(QString raw_packet);
    template<typename T> static void registerClass(QString header) { class_map[header] = &createInstance<T>; };

  private:
    template<typename T> static AOPacket* createInstance(QStringList contents) { return new T(contents); };
    template<typename T> static AOPacket* createInstanceDecode(QString raw_packet) { return new T(raw_packet); };
    typedef std::map<QString, AOPacket* (*)(QStringList)> type_map;

    static type_map class_map;
};