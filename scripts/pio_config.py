"""Keep an existing PlatformIO-generated sdkconfig compatible with the app."""

import collections.abc
import pathlib
import typing


class BuildEnvironment(typing.Protocol):
    """The SCons environment methods used by this pre-build script."""

    def subst(self, expression: str) -> str:
        """Expand a SCons build variable."""
        ...


def enable_software_aes(config: pathlib.Path) -> bool:
    """Update an existing generated sdkconfig without changing other options."""
    if not config.exists():
        return False  # A fresh build gets the setting from sdkconfig defaults.

    original = config.read_text(encoding="utf-8")
    lines = original.splitlines()
    selected = "# CONFIG_MBEDTLS_HARDWARE_AES is not set"
    matching = [
        line
        for line in lines
        if line.startswith("CONFIG_MBEDTLS_HARDWARE_AES=") or line == selected
    ]
    if matching == [selected]:
        return False

    result: list[str] = []
    replaced = False
    for line in lines:
        if line.startswith("CONFIG_MBEDTLS_HARDWARE_AES=") or line == selected:
            if not replaced:
                result.append(selected)
                replaced = True
        else:
            result.append(line)
    if not replaced:
        result.append(selected)
    updated = "\n".join(result)
    config.write_text(f"{updated}\n", encoding="utf-8")
    return True


def disable_xml_runtime(config: pathlib.Path) -> bool:
    """Disable XML while retaining the observer needed by LVGL's span widget."""
    if not config.exists():
        return False

    original = config.read_text(encoding="utf-8")
    lines = original.splitlines()
    for option, enabled in (
        ("CONFIG_LV_USE_XML", False),
        ("CONFIG_LV_USE_OBSERVER", True),
    ):
        disabled = f"# {option} is not set"
        desired = f"{option}=y" if enabled else disabled
        selected = [
            line for line in lines if line.startswith(f"{option}=") or line == disabled
        ]
        if selected == [desired]:
            continue
        result: list[str] = []
        replaced = False
        for line in lines:
            if line.startswith(f"{option}=") or line == disabled:
                if not replaced:
                    result.append(desired)
                    replaced = True
            else:
                result.append(line)
        if not replaced:
            result.append(desired)
        lines = result

    joined = "\n".join(lines)
    updated = f"{joined}\n"
    if updated == original:
        return False
    config.write_text(updated, encoding="utf-8")
    return True


def main() -> None:
    """Update existing settings using PlatformIO's injected environment."""
    # SCons injects Import and the requested env into this script's globals.
    import_environment = typing.cast(
        collections.abc.Callable[[str], None], globals()["Import"]
    )
    import_environment("env")
    env = typing.cast(BuildEnvironment, globals()["env"])
    root = pathlib.Path(env.subst("$PROJECT_DIR"))
    config = root / f"sdkconfig.{env.subst('$PIOENV')}"
    if enable_software_aes(config):
        print(f"Using software AES in {config.name} for encrypted settings.")
    if disable_xml_runtime(config):
        print(f"Disabled unused XML runtime in {config.name}.")


if "Import" in globals():
    main()
