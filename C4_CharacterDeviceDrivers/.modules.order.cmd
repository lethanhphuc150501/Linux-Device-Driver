cmd_/home/phucl/Linux-Device-Driver/C4_CharacterDeviceDrivers/modules.order := {   echo /home/phucl/Linux-Device-Driver/C4_CharacterDeviceDrivers/dummy_char.ko; :; } | awk '!x[$$0]++' - > /home/phucl/Linux-Device-Driver/C4_CharacterDeviceDrivers/modules.order