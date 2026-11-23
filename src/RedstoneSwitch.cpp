#include <atomic>

#ifdef REDSTONE_SWITCH_NATIVE_PULSE_26_60
#include <cstdint>
#include <cstring>
#include <Windows.h>
#endif

#include "ll/api/Versions.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/command/runtime/RuntimeOverload.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/command/ServerCommandRegisterEvent.h"
#include "ll/api/i18n/I18n.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/mod/RegisterHelper.h"
#include "mc/world/level/block/DispenserBlock.h"
#include "mc/world/level/block/ObserverBlock.h"
#include "mc/world/redstone/circuit/components/PulseCapacitor.h"

namespace redstone_switch {
namespace {

std::atomic_bool observerEnabled{true};
std::atomic_bool dispenserEnabled{true};

using ll::i18n_literals::operator""_tr;
using ll::memory::HookPriority;

LL_TYPE_INSTANCE_HOOK(
    ObserverStateHook,
    HookPriority::Normal,
    ObserverBlock,
    &ObserverBlock::_updateState,
    void,
    BlockSource& region,
    BlockPos const& pos,
    PulseCapacitor& component,
    bool turnOn
) {
    if (!turnOn || observerEnabled.load()) {
        origin(region, pos, component, turnOn);
    }
}

// Loading a powered observer can set its pulse without going through _updateState.
#ifdef REDSTONE_SWITCH_NATIVE_PULSE_26_60
// The 1.26.60.5 observer's circuit installation calls this PulseCapacitor vtable entry.
// LeviLamina 26.60.9 no longer publishes it in bedrockdata.
std::uintptr_t pulseStrengthTarget() {
    constexpr std::uintptr_t rva = 0x5EF1B10;
    constexpr unsigned char entry[] = {0x89, 0x51, 0x60, 0x85, 0xD2, 0x0F, 0x95, 0x41, 0x79, 0xC3};
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!image) {
        return 0;
    }
    const auto address = image + rva;
    const auto* target = reinterpret_cast<void const*>(address);
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(target, &info, sizeof(info)) || reinterpret_cast<std::uintptr_t>(info.AllocationBase) != image
        || info.State != MEM_COMMIT || !(info.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))
        || (info.Protect & PAGE_GUARD)
        || address + sizeof(entry) > reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize) {
        return 0;
    }
    return std::memcmp(target, entry, sizeof(entry)) == 0 ? address : 0;
}
#define REDSTONE_SWITCH_PULSE_TARGET pulseStrengthTarget()
#else
#define REDSTONE_SWITCH_PULSE_TARGET &PulseCapacitor::$setStrength
#endif

LL_TYPE_INSTANCE_HOOK(
    ObserverPulseHook,
    HookPriority::Normal,
    PulseCapacitor,
    REDSTONE_SWITCH_PULSE_TARGET,
    void,
    int strength
) {
    origin(observerEnabled.load() ? strength : 0);
}
#undef REDSTONE_SWITCH_PULSE_TARGET

LL_TYPE_INSTANCE_HOOK(
    DispenserHook,
    HookPriority::Normal,
    DispenserBlock,
    &DispenserBlock::$dispenseFrom,
    void,
    BlockSource& region,
    BlockPos const& pos
) {
    if (dispenserEnabled.load()) {
        origin(region, pos);
    }
}

void registerCommands(ll::event::command::ServerCommandRegisterEvent const& event) {
    if (!event.isClientHostedServer()) {
        return;
    }

    observerEnabled.store(true);
    dispenserEnabled.store(true);

    auto& command = ll::command::CommandRegistrar::getServerInstance().getOrCreateCommand(
        "rsrule",
        "Toggle observer redstone output and dispenser item output."_tr(),
        CommandPermissionLevel::GameDirectors
    );

    command.runtimeOverload()
        .text("observer")
        .required("disabled", ll::command::ParamKind::Bool)
        .execute([](CommandOrigin const&, CommandOutput& output, ll::command::RuntimeCommand const& args) {
            const bool disabled = args["disabled"].get<ll::command::ParamKind::Bool>();
            observerEnabled.store(!disabled);
            output.success(
                disabled ? "Observer redstone output is disabled."_tr() : "Observer redstone output is enabled."_tr()
            );
        });

    command.runtimeOverload()
        .text("dispenser")
        .required("disabled", ll::command::ParamKind::Bool)
        .execute([](CommandOrigin const&, CommandOutput& output, ll::command::RuntimeCommand const& args) {
            const bool disabled = args["disabled"].get<ll::command::ParamKind::Bool>();
            dispenserEnabled.store(!disabled);
            output.success(
                disabled ? "Dispenser item output is disabled."_tr() : "Dispenser item output is enabled."_tr()
            );
        });
}

} // namespace

class RedstoneSwitch {
    ll::mod::NativeMod& mSelf;
    ll::event::ListenerPtr mCommandListener;

public:
    RedstoneSwitch() : mSelf(*ll::mod::NativeMod::current()) {}

    static RedstoneSwitch& getInstance() {
        static RedstoneSwitch instance;
        return instance;
    }

    bool load() {
        const auto loaderVersion = ll::getLoaderVersion();
        if (loaderVersion != ll::data::Version{REDSTONE_SWITCH_LEVILAMINA_VERSION}) {
            mSelf.getLogger().error(
                "RedstoneSwitch requires LeviLamina {}, found {}",
                REDSTONE_SWITCH_LEVILAMINA_VERSION,
                loaderVersion.to_string()
            );
            return false;
        }
        if (auto result = ll::i18n::getInstance().load(mSelf.getLangDir()); !result) {
            mSelf.getLogger().warn("Could not load RedstoneSwitch translations: {}", result.error().message());
        }
        return true;
    }

    bool enable() {
        if (ObserverStateHook::hook() != 0) {
            mSelf.getLogger().error("Failed to hook ObserverBlock::_updateState");
            return false;
        }
        if (ObserverPulseHook::hook() != 0) {
            ObserverStateHook::unhook();
            mSelf.getLogger().error("Failed to hook PulseCapacitor::setStrength");
            return false;
        }
        if (DispenserHook::hook() != 0) {
            ObserverPulseHook::unhook();
            ObserverStateHook::unhook();
            mSelf.getLogger().error("Failed to hook DispenserBlock::dispenseFrom");
            return false;
        }

        mCommandListener = ll::event::EventBus::getInstance()
                               .emplaceListener<ll::event::command::ServerCommandRegisterEvent>(registerCommands);
        if (!mCommandListener) {
            disable();
            mSelf.getLogger().error("Failed to listen for server command registration");
            return false;
        }

        mSelf.getLogger().info("RedstoneSwitch enabled");
        return true;
    }

    bool disable() {
        if (mCommandListener) {
            ll::event::EventBus::getInstance().removeListener(mCommandListener);
            mCommandListener.reset();
        }
        observerEnabled.store(true);
        dispenserEnabled.store(true);
        DispenserHook::unhook();
        ObserverPulseHook::unhook();
        ObserverStateHook::unhook();
        return true;
    }
};

} // namespace redstone_switch

LL_REGISTER_MOD(redstone_switch::RedstoneSwitch, redstone_switch::RedstoneSwitch::getInstance());
