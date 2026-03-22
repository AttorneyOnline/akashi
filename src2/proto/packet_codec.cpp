#include "proto/packet_codec.h"

namespace akashi {

std::unique_ptr<Message> DropCodec::decode(const Packet &f_packet) const
{
    Q_UNUSED(f_packet)
    return nullptr;
}

CodecRule always()
{
    return [](const ClientProfile &) { return true; };
}

CodecRule archIs(const QString &f_arch)
{
    return [f_arch](const ClientProfile &f_profile) { return f_profile.arch == f_arch; };
}

CodecRule versionAtLeast(int f_release, int f_major, int f_minor)
{
    return [f_release, f_major, f_minor](const ClientProfile &f_profile) {
        return f_profile.version.atLeast(f_release, f_major, f_minor);
    };
}

CodecRule hasFeature(const QString &f_feature)
{
    return [f_feature](const ClientProfile &f_profile) { return f_profile.hasFeature(f_feature); };
}

CodecRule allOf(const QList<CodecRule> &f_rules)
{
    return [f_rules](const ClientProfile &f_profile) {
        for (const CodecRule &l_rule : f_rules) {
            if (!l_rule(f_profile)) {
                return false;
            }
        }
        return true;
    };
}

CodecRule anyOf(const QList<CodecRule> &f_rules)
{
    return [f_rules](const ClientProfile &f_profile) {
        for (const CodecRule &l_rule : f_rules) {
            if (l_rule(f_profile)) {
                return true;
            }
        }
        return false;
    };
}

void ResolvedCodecs::setDefault(std::shared_ptr<Codec> f_codec)
{
    m_default = f_codec;
}

void ResolvedCodecs::set(const QString &f_header, std::shared_ptr<Codec> f_codec)
{
    m_codecs.insert(f_header, f_codec);
}

std::shared_ptr<Codec> ResolvedCodecs::codecFor(const QString &f_header) const
{
    return m_codecs.value(f_header, m_default);
}

void PacketCodecRegistry::registerCodec(const QString &f_header, CodecRule f_rule, int f_priority,
                                        std::shared_ptr<Codec> f_codec, const QString &f_owner_id)
{
    m_entries[f_header].append(Entry{f_rule, f_priority, m_counter++, f_codec, f_owner_id});
}

void PacketCodecRegistry::unregisterAll(const QString &f_owner_id)
{
    for (auto l_iterator = m_entries.begin(); l_iterator != m_entries.end(); ++l_iterator) {
        QList<Entry> &l_entries = l_iterator.value();
        l_entries.erase(std::remove_if(l_entries.begin(), l_entries.end(),
                                       [&f_owner_id](const Entry &f_entry) { return f_entry.owner == f_owner_id; }),
                        l_entries.end());
    }
}

std::shared_ptr<Codec> PacketCodecRegistry::best(const QString &f_header, const ClientProfile &f_profile) const
{
    const Entry *l_best = nullptr;
    // Both the header's own codecs and the wildcard default codecs are candidates.
    for (const QString &l_key : {f_header, QStringLiteral("*")}) {
        const auto l_iterator = m_entries.constFind(l_key);
        if (l_iterator == m_entries.constEnd()) {
            continue;
        }
        for (const Entry &l_entry : l_iterator.value()) {
            if (!l_entry.rule(f_profile)) {
                continue;
            }
            // Highest priority wins; a later registration breaks ties.
            if (!l_best || l_entry.priority > l_best->priority ||
                (l_entry.priority == l_best->priority && l_entry.order > l_best->order)) {
                l_best = &l_entry;
            }
        }
    }
    return l_best ? l_best->codec : nullptr;
}

ResolvedCodecs PacketCodecRegistry::resolve(const ClientProfile &f_profile) const
{
    ResolvedCodecs l_resolved;
    l_resolved.setDefault(best(QStringLiteral("*"), f_profile));
    for (auto l_iterator = m_entries.constBegin(); l_iterator != m_entries.constEnd(); ++l_iterator) {
        if (l_iterator.key() == QStringLiteral("*")) {
            continue;
        }
        l_resolved.set(l_iterator.key(), best(l_iterator.key(), f_profile));
    }
    return l_resolved;
}

} // namespace akashi
