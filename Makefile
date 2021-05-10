APPLICATION = sleepy_node

# add "DEVELHELP=1" to the call to the make command
# to enable safety checking and error reporting
DEVELHELP=1

BOARD ?= samr21-xpro

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../RIOT

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

# we are a CoAP client
USEMODULE += gcoap

ifeq ($(BOARD),native)
  USEMODULE += socket_zep
  TERMFLAGS += -z [::1]:17754

  ifneq (,$(ZEP_MAC))
    TERMFLAGS += --eui64=$(ZEP_MAC)
  endif
else
  # make sure we don't always start with the same random seed
  USEMODULE += puf_sram
endif

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
