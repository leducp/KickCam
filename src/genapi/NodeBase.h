#ifndef KICKCAM_GENAPI_NODE_BASE_H
#define KICKCAM_GENAPI_NODE_BASE_H

#include "kickcam/genapi/Node.h"
#include "genapi/Value.h"

namespace kickcam::genapi
{
    // The most restrictive of two accesses: RO and WO together give NA.
    Access combine(Access a, Access b);

    // Fails on an error, and on NI or NA.
    std::expected<void, std::error_code> check_usable(std::expected<Access, std::error_code> mode);

    // Fails on an error, NI, NA and WO.
    std::expected<void, std::error_code> check_readable(std::expected<Access, std::error_code> mode);

    // pIsImplemented, pIsAvailable, pIsLocked and ImposedAccessMode of a node.
    struct Conditions
    {
        // NI or NA when pIsImplemented or pIsAvailable is false, RW otherwise.
        std::expected<Access, std::error_code> presence() const;

        // Turns a writable access into a read-only one while pIsLocked is true.
        std::expected<Access, std::error_code> lock(std::expected<Access, std::error_code> unlocked) const;

        std::expected<void, std::error_code> check_writable(std::expected<Access, std::error_code> unlocked) const;

        IntValue is_implemented{int64_t{1}};
        IntValue is_available{int64_t{1}};
        IntValue is_locked{int64_t{0}};
        Access imposed{Access::RW};
    };

    template<typename Interface>
    class NodeBase : public Interface
    {
    public:
        NodeBase(std::string name)
            : name_{std::move(name)}
        {
        }

        std::string const& name() const override
        {
            return name_;
        }

        std::expected<Access, std::error_code> access() const override
        {
            return conditions.lock(unlocked_access());
        }

        Conditions conditions;

    protected:
        virtual std::expected<Access, std::error_code> own_access() const
        {
            return Access::RW;
        }

        std::expected<void, std::error_code> check_readable() const
        {
            return genapi::check_readable(access());
        }

        std::expected<void, std::error_code> check_writable() const
        {
            return conditions.check_writable(unlocked_access());
        }

    private:
        // The node's own access is read only when the node is implemented and available.
        std::expected<Access, std::error_code> unlocked_access() const
        {
            auto presence = conditions.presence();
            if (not presence or *presence != Access::RW)
            {
                return presence;
            }
            return own_access().transform([this](Access own) { return combine(own, conditions.imposed); });
        }

        std::string name_;
    };

    // Checks access, min and max (and inc for integers) around the read and write of Integer and Float nodes.
    template<typename Interface, typename T>
    class NumericBase : public NodeBase<Interface>
    {
    public:
        using NodeBase<Interface>::NodeBase;

        std::expected<T, std::error_code> get() const final;
        std::expected<void, std::error_code> set(T value) final;

    protected:
        virtual std::expected<T, std::error_code> read() const = 0;
        virtual std::expected<void, std::error_code> write(T value) = 0;
    };
}

#endif
