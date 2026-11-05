Import("env")
from version import version

# version.py is the one place the version is set. The firmware reports it from this macro.
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])
