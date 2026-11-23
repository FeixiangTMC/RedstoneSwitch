set_project("RedstoneSwitch")
set_version("1.0.0")
set_xmakever("3.0.0")
set_allowedplats("windows")
set_allowedarchs("x64")
set_defaultarchs("x64")
add_rules("mode.debug", "mode.release")

option("ll_version")
    set_default("26.51.9")
    set_values("26.51.9", "26.60.9")
    set_showmenu(true)
    set_description("Choose the LeviLamina client SDK version")
option_end()

local loader_version = get_config("ll_version") or "26.51.9"
local package_dirs = {
    ["26.51.9"] = "bin",
    ["26.60.9"] = "bin/1.26.60.5"
}
local package_dir = package_dirs[loader_version]

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")
add_requires("levilamina " .. loader_version, {configs = {target_type = "client"}})
add_requires("levibuildscript 0.6.1")

target("RedstoneSwitch")
    set_kind("shared")
    set_languages("c++20")
    set_toolchains("clang-cl")
    set_runtimes("MD")
    set_symbols("debug")
    add_defines("NOMINMAX", "UNICODE", "_HAS_CXX23=1")
    add_defines('REDSTONE_SWITCH_LEVILAMINA_VERSION="' .. loader_version .. '"')
    if loader_version == "26.60.9" then
        add_defines("REDSTONE_SWITCH_NATIVE_PULSE_26_60")
    end
    add_cxflags("/utf-8", "/EHs", {tools = {"clang_cl"}})
    add_rules("@levibuildscript/linkrule")
    add_packages("levilamina")
    add_files("src/*.cpp")

    after_build(function (target)
        local moddir = path.join(os.projectdir(), package_dir, target:name())
        os.mkdir(moddir)
        os.cp(target:targetfile(), moddir)
        os.cp(path.join(os.projectdir(), "manifest.json"), moddir)
        local langdir = path.join(moddir, "lang")
        os.mkdir(langdir)
        for _, langfile in ipairs(os.files(path.join(os.projectdir(), "src", "lang", "*.json"))) do
            os.cp(langfile, langdir)
        end
    end)
