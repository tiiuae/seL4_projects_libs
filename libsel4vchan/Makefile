TARGETS := libsel4vchan.a

CFILES := $(patsubst $(SOURCE_DIR)/%,%,$(wildcard ${SOURCE_DIR}/src/*.c))

# Header files/directories this library provides
HDRFILES := $(wildcard ${SOURCE_DIR}/include/*)

include $(SEL4_COMMON)/common.mk
