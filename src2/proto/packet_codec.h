#ifndef PROTO_PACKET_CODEC_H
#define PROTO_PACKET_CODEC_H

#include "akashi_core_export.h"
#include "proto/client_profile.h"
#include "proto/message.h"
#include "proto/packet.h"

#include <QHash>
#include <QList>

#include <functional>
#include <memory>

class QThread;

namespace akashi {

// Translates one packet type between the network protocol and its Message, for one dialect.
class AKASHI_CORE_EXPORT Codec
{
  public:
    virtual ~Codec() = default;
    virtual std::unique_ptr<Message> decode(const Packet &f_packet) const = 0;

    // Until server-to-client traffic is built from messages, codecs only decode.
    virtual Packet encode(const Message &f_message) const;
};

// Future-proofing: decodes nothing. Registered under a feature-gated
// codec, it makes a packet from a client outside that feature go nowhere
// instead of falling back to the wildcard codec's empty message.
class AKASHI_CORE_EXPORT DropCodec : public Codec
{
  public:
    std::unique_ptr<Message> decode(const Packet &f_packet) const override;
};

// Decides whether a codec applies to a connection. Composable, like a config check.
using CodecRule = std::function<bool(const ClientProfile &)>;

// Rule building blocks.
AKASHI_CORE_EXPORT CodecRule always();
AKASHI_CORE_EXPORT CodecRule archIs(const QString &f_arch);
AKASHI_CORE_EXPORT CodecRule versionAtLeast(int f_release, int f_major, int f_minor);
AKASHI_CORE_EXPORT CodecRule hasFeature(const QString &f_feature);
AKASHI_CORE_EXPORT CodecRule allOf(const QList<CodecRule> &f_rules);
AKASHI_CORE_EXPORT CodecRule anyOf(const QList<CodecRule> &f_rules);

// The codecs chosen for one connection. Built once, then looked up per packet.
class AKASHI_CORE_EXPORT ResolvedCodecs
{
  public:
    void setDefault(std::shared_ptr<Codec> f_codec);
    void set(const QString &f_header, std::shared_ptr<Codec> f_codec);

    // The codec for a header, falling back to the default.
    std::shared_ptr<Codec> codecFor(const QString &f_header) const;

  private:
    QHash<QString, std::shared_ptr<Codec>> m_codecs;
    std::shared_ptr<Codec> m_default;
};

// Holds every registered codec and resolves them for a connection.
// A header of "*" registers a default codec that applies to any packet.
class AKASHI_CORE_EXPORT PacketCodecRegistry
{
  public:
    PacketCodecRegistry();

    // Higher priority wins; ties are broken by the later registration.
    void registerCodec(const QString &f_header, CodecRule f_rule, int f_priority,
                       std::shared_ptr<Codec> f_codec, const QString &f_owner_id = QString());

    // Removes every codec registered by the given owner, for plugin unloading.
    void unregisterAll(const QString &f_owner_id);

    // Picks the winning codec per header for the given client. Run once at ID time.
    ResolvedCodecs resolve(const ClientProfile &f_profile) const;

  private:
    struct Entry
    {
        CodecRule rule;
        int priority;
        int order;
        std::shared_ptr<Codec> codec;
        QString owner;
    };

    std::shared_ptr<Codec> best(const QString &f_header, const ClientProfile &f_profile) const;

    QHash<QString, QList<Entry>> m_entries;
    int m_counter = 0;
    QThread *m_owner_thread;
};

} // namespace akashi

#endif // PROTO_PACKET_CODEC_H
