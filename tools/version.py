# PlatformIO-Skript (vor dem Bauen): Versionsnummer aus Git als FIRMWARE_VERSION.
# In GitHub Actions kann sie über die Umgebungsvariable FIRMWARE_VERSION vorgegeben werden.
import os
import subprocess

Import("env")  # noqa: F821 (von PlatformIO bereitgestellt)

version = os.environ.get("FIRMWARE_VERSION")
if not version:
    try:
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"], cwd=env.subst("$PROJECT_DIR"), text=True
        ).strip()
    except Exception:
        version = "unbekannt"

env.Append(CPPDEFINES=[("FIRMWARE_VERSION", '\\"%s\\"' % version)])  # noqa: F821
print("Firmware-Version:", version)
