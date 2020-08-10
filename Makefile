APPLICATION = sleepy_node

# add "DEVELHELP=1" to the call to the make command
# to enable safety checking and error reporting
DEVELHELP=1

BOARD ?= samr21-xpro

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# We neet RTC to wake from hibernate
FEATURES_REQUIRED += periph_rtc

# Modules to include:
USEMODULE += shell_commands

# gnrc is a meta module including all required, basic gnrc networking modules
USEMODULE += gnrc
USEMODULE += gnrc_netdev_default

# manually initialize the network interface
USEMODULE += gnrc_netif_init_devs

# pull in the GNRC IP stack
USEMODULE += gnrc_ipv6_default

# enable GNRC notifications
USEMODULE += gnrc_netif_bus

# only print on error
CFLAGS += -DCONFIG_SKIP_BOOT_MSG

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk