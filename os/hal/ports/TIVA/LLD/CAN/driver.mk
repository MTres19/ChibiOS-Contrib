ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_CAN TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS_CONTRIB)/os/hal/ports/TIVA/LLD/CAN/hal_can_lld.c
endif
else
PLATFORMSRC += $(CHIBIOS_CONTRIB)/os/hal/ports/TIVA/LLD/CAN/hal_can_lld.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/TIVA/LLD/CAN
