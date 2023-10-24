# get the OS Name
UNAME_S := $(shell uname -s)

# Get git commit version and date
GIT_DATE := $(firstword $(shell git --no-pager show --date=short --format="%ai" --name-only))
GIT_VERSION := $(shell git describe --abbrev=0 --tags --always)

# uncomment the line below to include GPIO (needs libgpiod)
GPIO_INCLUDE=GPIO

# uncomment the line below to include MIDI support (needs MIDI support)
MIDI_INCLUDE=MIDI

# uncomment the line below to include SATURN native xdma support
SATURN_INCLUDE=SATURN

# uncomment the line below to include USB Ozy support (needs libusb)
#USBOZY_INCLUDE=USBOZY

# uncomment the line below for SoapySDR (needs SOAPY libs)
#SOAPYSDR_INCLUDE=SOAPYSDR

# uncomment the line below to include support for STEMlab discovery (needs libcurl)
#STEMLAB_DISCOVERY=STEMLAB

# uncomment to get ALSA audio module on Linux (if not, use pulseaudio)
#AUDIO_MODULE=ALSA

# un-comment if you link with an extended WDSP library containing new "external"
# noise reduction capabilities ("rnnoise" and "libspecbleach")
# (see: github.com/vu3rdd/wdsp). To use this, an extended version of WDSP has
# to be installed that contains calls to the "rnnoise" and "libspecbleach"
# libraries. The new features are called "NR3" (rnnoise) and "NR4" (libspecbleach),
# and the noise menu allows to speficy the NR4 parameters.
#EXTENDED_NOISE_REDUCTION=EXTNR

# very early code not included yet
#SERVER_INCLUDE=SERVER

###############################################################################
#
# NO COMPILE TIME OPTION MODIFICATIONS BELOW THIS LINE
#
###############################################################################

CFLAGS?= -O3 -Wno-deprecated-declarations -Wall
LINK?=   $(CC)
#
# The "official" way to compile+link with pthreads is now to use the -pthread option
# *both* for the compile and the link step.
#
CFLAGS+=-pthread -I./src
LINK+=-pthread

PKG_CONFIG = pkg-config

##############################################################################
#
# Settings for optional features, to be requested by un-commenting lines above
#
##############################################################################

#
# disable GPIO and SATURN for MacOS, simply because it is not there
#
ifeq ($(UNAME_S), Darwin)
GPIO_INCLUDE=
SATURN_INCLUDE=
endif

#
# Add modules for MIDI if requested.
# Note these are different for Linux/MacOS
#
ifeq ($(MIDI_INCLUDE),MIDI)
MIDI_OPTIONS=-D MIDI
MIDI_HEADERS= midi.h midi_menu.h alsa_midi.h
ifeq ($(UNAME_S), Darwin)
MIDI_SOURCES= src/mac_midi.c src/midi2.c src/midi3.c src/midi_menu.c
MIDI_OBJS= src/mac_midi.o src/midi2.o src/midi3.o src/midi_menu.o
MIDI_LIBS= -framework CoreMIDI -framework Foundation
endif
ifeq ($(UNAME_S), Linux)
MIDI_SOURCES= src/alsa_midi.c src/midi2.c src/midi3.c src/midi_menu.c
MIDI_OBJS= src/alsa_midi.o src/midi2.o src/midi3.o src/midi_menu.o
MIDI_LIBS= -lasound
endif
endif

#
# Add libraries for Saturn support, if requested
#
ifeq ($(SATURN_INCLUDE),SATURN)
SATURN_OPTIONS=-D SATURN
SATURN_SOURCES= \
src/saturndrivers.c \
src/saturnregisters.c \
src/saturnserver.c \
src/saturnmain.c \
src/saturn_menu.c
SATURN_HEADERS= \
src/saturndrivers.h \
src/saturnregisters.h \
src/saturnserver.h \
src/saturnmain.h \
src/saturn_menu.h
SATURN_OBJS= \
src/saturndrivers.o \
src/saturnregisters.o \
src/saturnserver.o \
src/saturnmain.o \
src/saturn_menu.o
endif

#
# Add libraries for USB OZY support, if requested
#
ifeq ($(USBOZY_INCLUDE),USBOZY)
USBOZY_OPTIONS=-D USBOZY
USBOZY_LIBS=-lusb-1.0
USBOZY_SOURCES= \
src/ozyio.c
USBOZY_HEADERS= \
src/ozyio.h
USBOZY_OBJS= \
src/ozyio.o
endif

#
# Add libraries for SoapySDR support, if requested
#
ifeq ($(SOAPYSDR_INCLUDE),SOAPYSDR)
SOAPYSDR_OPTIONS=-D SOAPYSDR
SOAPYSDRLIBS=-lSoapySDR
SOAPYSDR_SOURCES= \
src/soapy_discovery.c \
src/soapy_protocol.c
SOAPYSDR_HEADERS= \
src/soapy_discovery.h \
src/soapy_protocol.h
SOAPYSDR_OBJS= \
src/soapy_discovery.o \
src/soapy_protocol.o
endif

#
# Add support for extended noise reduction, if requested
#
ifeq ($(EXTENDED_NOISE_REDUCTION), EXTNR)
EXTNR_OPTIONS=-DEXTNR
endif

#
# Add libraries for GPIO support, if requested
#
ifeq ($(GPIO_INCLUDE),GPIO)
GPIO_OPTIONS=-D GPIO
GPIOD_VERSION=$(shell pkg-config --modversion libgpiod)
ifeq ($(GPIOD_VERSION),1.2)
GPIO_OPTIONS += -D OLD_GPIOD
endif
GPIO_LIBS=-lgpiod -li2c
endif

#
# Activate code for RedPitaya (Stemlab/Hamlab/plain vanilla), if requested
# This code detects the RedPitaya by its WWW interface and starts the SDR
# application.
# If the RedPitaya auto-starts the SDR application upon system start,
# this option is not needed!
#
ifeq ($(STEMLAB_DISCOVERY), STEMLAB)
STEMLAB_OPTIONS=-D STEMLAB_DISCOVERY `$(PKG_CONFIG) --cflags libcurl`
STEMLAB_LIBS=`$(PKG_CONFIG) --libs libcurl`
STEMLAB_SOURCES=src/stemlab_discovery.c
STEMLAB_HEADERS=src/stemlab_discovery.h
STEMLAB_OBJS=src/stemlab_discovery.o
endif

#
# Activate code for remote operation, if requested.
# This feature is not yet finished. If finished, it
# allows to run two instances of piHPSDR on two
# different computers, one interacting with the operator
# and the other talking to the radio, and both computers
# may be connected by a long-distance internet connection.
#
ifeq ($(SERVER_INCLUDE), SERVER)
SERVER_OPTIONS=-D CLIENT_SERVER
SERVER_SOURCES= \
src/client_server.c src/server_menu.c
SERVER_HEADERS= \
src/client_server.h src/server_menu.h
SERVER_OBJS= \
src/client_server.o src/server_menu.o
endif

#
# Options for audio module
#  - MacOS: only PORTAUDIO tested (although PORTAUDIO might work)
#  - Linux: either PULSEAUDIO (default) or ALSA (upon request)
#
ifeq ($(UNAME_S), Darwin)
    AUDIO_MODULE=PORTAUDIO
endif
ifeq ($(UNAME_S), Linux)
  ifeq ($(AUDIO_MODULE) , ALSA)
    AUDIO_MODULE=ALSA
  else
    AUDIO_MODULE=PULSEAUDIO
  endif
endif

#
# Add libraries for using PulseAudio, if requested
#
ifeq ($(AUDIO_MODULE), PULSEAUDIO)
AUDIO_OPTIONS=-DPULSEAUDIO
AUDIO_LIBS=-lpulse-simple -lpulse -lpulse-mainloop-glib
AUDIO_SOURCES=src/pulseaudio.c
AUDIO_OBJS=src/pulseaudio.o
endif

#
# Add libraries for using ALSA, if requested
#
ifeq ($(AUDIO_MODULE), ALSA)
AUDIO_OPTIONS=-DALSA
AUDIO_LIBS=-lasound
AUDIO_SOURCES=src/audio.c
AUDIO_OBJS=src/audio.o
endif

#
# Add libraries for using PortAudio, if requested
#
ifeq ($(AUDIO_MODULE), PORTAUDIO)
AUDIO_OPTIONS=-DPORTAUDIO `$(PKG_CONFIG) --cflags portaudio-2.0`
AUDIO_LIBS=`$(PKG_CONFIG) --libs portaudio-2.0`
AUDIO_SOURCES=src/portaudio.c
AUDIO_OBJS=src/portaudio.o
endif

##############################################################################
#
# End of "libraries for optional features" section
#
##############################################################################

#
# Includes and Libraries for the graphical user interface (GTK)
#
GTKINCLUDES=`$(PKG_CONFIG) --cflags gtk+-3.0`
GTKLIBS=`$(PKG_CONFIG) --libs gtk+-3.0`

#
# Specify additional OS-dependent system libraries
#
ifeq ($(UNAME_S), Linux)
SYSLIBS=-lrt
endif

ifeq ($(UNAME_S), Darwin)
SYSLIBS=-framework IOKit
endif

#
# All the command-line options to compile the *.c files
#
OPTIONS=$(MIDI_OPTIONS) $(USBOZY_OPTIONS) \
	$(GPIO_OPTIONS) $(SOAPYSDR_OPTIONS) \
	$(ANDROMEDA_OPTIONS) \
	$(SATURN_OPTIONS) \
	$(STEMLAB_OPTIONS) \
	$(SERVER_OPTIONS) \
	$(AUDIO_OPTIONS) $(EXTNR_OPTIONS)\
	-D GIT_DATE='"$(GIT_DATE)"' -D GIT_VERSION='"$(GIT_VERSION)"' $(DEBUG_OPTION)

INCLUDES=$(GTKINCLUDES)
COMPILE=$(CC) $(CFLAGS) $(OPTIONS) $(INCLUDES)

.c.o:
	$(COMPILE) -c -o $@ $<

#
# All the libraries we need to link with (including WDSP, libm, $(SYSLIBS))
#
LIBS=	$(LDFLAGS) $(AUDIO_LIBS) $(USBOZY_LIBS) $(GTKLIBS) $(GPIO_LIBS) $(SOAPYSDRLIBS) $(STEMLAB_LIBS) \
	$(MIDI_LIBS) -lwdsp -lm $(SYSLIBS)

#
# The main target, the pihpsdr program
#
PROGRAM=pihpsdr

#
# The core *.c files in alphabetical order
#
SOURCES= \
src/MacOS.c \
src/about_menu.c \
src/actions.c \
src/action_dialog.c \
src/agc_menu.c \
src/ant_menu.c \
src/appearance.c \
src/band.c \
src/band_menu.c \
src/bandstack_menu.c \
src/css.c \
src/configure.c \
src/cw_menu.c \
src/cwramp.c \
src/discovered.c \
src/discovery.c \
src/display_menu.c \
src/diversity_menu.c \
src/encoder_menu.c \
src/equalizer_menu.c \
src/exit_menu.c \
src/ext.c \
src/fft_menu.c \
src/filter.c \
src/filter_menu.c \
src/gpio.c \
src/i2c.c \
src/iambic.c \
src/led.c \
src/main.c \
src/message.c \
src/meter.c \
src/meter_menu.c \
src/mode.c \
src/mode_menu.c \
src/mystring.c \
src/new_discovery.c \
src/new_menu.c \
src/new_protocol.c \
src/noise_menu.c \
src/oc_menu.c \
src/old_discovery.c \
src/old_protocol.c \
src/pa_menu.c \
src/property.c \
src/protocols.c \
src/ps_menu.c \
src/radio.c \
src/radio_menu.c \
src/receiver.c \
src/rigctl.c \
src/rigctl_menu.c \
src/rx_menu.c \
src/rx_panadapter.c \
src/screen_menu.c \
src/sintab.c \
src/sliders.c \
src/store.c \
src/store_menu.c \
src/switch_menu.c \
src/toolbar.c \
src/toolbar_menu.c \
src/transmitter.c \
src/tx_menu.c \
src/tx_panadapter.c \
src/version.c \
src/vfo.c \
src/vfo_menu.c \
src/vox.c \
src/vox_menu.c \
src/waterfall.c \
src/xvtr_menu.c \
src/zoompan.c

#
# The core *.h (header) files in alphabetical order
#
HEADERS= \
src/MacOS.h \
src/about_menu.h \
src/actions.h \
src/action_dialog.h \
src/adc.h \
src/agc.h \
src/agc_menu.h \
src/alex.h \
src/ant_menu.h \
src/appearance.h \
src/band.h \
src/band_menu.h \
src/bandstack_menu.h \
src/bandstack.h \
src/channel.h \
src/configure.h \
src/css.h \
src/cw_menu.h \
src/dac.h \
src/discovered.h \
src/discovery.h \
src/display_menu.h \
src/diversity_menu.h \
src/encoder_menu.h \
src/equalizer_menu.h \
src/exit_menu.h \
src/ext.h \
src/fft_menu.h \
src/filter.h \
src/filter_menu.h \
src/gpio.h \
src/iambic.h \
src/i2c.h \
src/led.h \
src/main.h \
src/message.h \
src/meter.h \
src/meter_menu.h \
src/mode.h \
src/mode_menu.h \
src/mystring.h \
src/new_discovery.h \
src/new_menu.h \
src/new_protocol.h \
src/noise_menu.h \
src/oc_menu.h \
src/old_discovery.h \
src/old_protocol.h \
src/pa_menu.h \
src/property.h \
src/protocols.h \
src/ps_menu.h \
src/radio.h \
src/radio_menu.h \
src/receiver.h \
src/rigctl.h \
src/rigctl_menu.h \
src/rx_menu.h \
src/rx_panadapter.h \
src/screen_menu.h \
src/sintab.h \
src/sliders.h \
src/store.h \
src/store_menu.h \
src/switch_menu.h \
src/toolbar.h \
src/toolbar_menu.h \
src/transmitter.h \
src/tx_menu.h \
src/tx_panadapter.h \
src/version.h \
src/vfo.h \
src/vfo_menu.h \
src/vox.h \
src/vox_menu.h \
src/waterfall.h \
src/xvtr_menu.h \
src/zoompan.h

#
# The core *.o (object) files in alphabetical order
#
OBJS= \
src/MacOS.o \
src/about_menu.o \
src/actions.o \
src/action_dialog.o \
src/agc_menu.o \
src/ant_menu.o \
src/appearance.o \
src/band.o \
src/band_menu.o \
src/bandstack_menu.o \
src/configure.o \
src/css.o \
src/cw_menu.o \
src/cwramp.o \
src/discovered.o \
src/discovery.o \
src/display_menu.o \
src/diversity_menu.o \
src/encoder_menu.o \
src/equalizer_menu.o \
src/exit_menu.o \
src/ext.o \
src/fft_menu.o \
src/filter.o \
src/filter_menu.o \
src/gpio.o \
src/iambic.o \
src/i2c.o \
src/led.o \
src/main.o \
src/message.o \
src/meter.o \
src/meter_menu.o \
src/mode.o \
src/mode_menu.o \
src/mystring.o \
src/new_discovery.o \
src/new_menu.o \
src/new_protocol.o \
src/noise_menu.o \
src/oc_menu.o \
src/old_discovery.o \
src/old_protocol.o \
src/pa_menu.o \
src/property.o \
src/protocols.o \
src/ps_menu.o \
src/radio.o \
src/radio_menu.o \
src/receiver.o \
src/rigctl.o \
src/rigctl_menu.o \
src/rx_menu.o \
src/rx_panadapter.o \
src/screen_menu.o \
src/sintab.o \
src/sliders.o \
src/store.o \
src/store_menu.o \
src/switch_menu.o \
src/toolbar.o \
src/toolbar_menu.o \
src/transmitter.o \
src/tx_menu.o \
src/tx_panadapter.o \
src/version.o \
src/vfo.o \
src/vfo_menu.o \
src/vox.o \
src/vox_menu.o \
src/xvtr_menu.o \
src/waterfall.o \
src/zoompan.o

#
# How to link the program
#
$(PROGRAM):  $(OBJS) $(AUDIO_OBJS) $(USBOZY_OBJS) $(SOAPYSDR_OBJS) \
		$(MIDI_OBJS) $(STEMLAB_OBJS) $(SERVER_OBJS) $(SATURN_OBJS)
	$(LINK) -o $(PROGRAM) $(OBJS) $(AUDIO_OBJS) $(USBOZY_OBJS) $(SOAPYSDR_OBJS) \
		$(MIDI_OBJS) $(STEMLAB_OBJS) $(SERVER_OBJS) $(SATURN_OBJS) $(LIBS)

.PHONY:	all
all:	prebuild  $(PROGRAM) $(HEADERS) $(AUDIO_HEADERS) $(USBOZY_HEADERS) $(SOAPYSDR_HEADERS) \
	$(MIDI_HEADERS) $(STEMLAB_HEADERS) $(SERVER_HEADERS) $(AUDIO_SOURCES) $(SOURCES) \
	$(USBOZY_SOURCES) $(SOAPYSDR_SOURCES) $(SATURN_HEADERS) $(SATURN_SOURCES) \
	$(MIDI_SOURCES) $(STEMLAB_SOURCES) $(SERVER_SOURCES)

.PHONY:	prebuild
prebuild:
	rm -f src/version.o

#
# "make check" invokes the cppcheck program to do a source-code checking.
#
# The "-pthread" compiler option is not valid for cppcheck and must be filtered out.
# Furthermore, we can add additional options to cppcheck in the variable CPPOPTIONS
#
# Normally cppcheck complains about variables that could be declared "const".
# Suppress this warning for callback functions because adding "const" would need
# an API change in many cases.
#
# On MacOS, cppcheck usually cannot find the system include files so we suppress any
# warnings therefrom. Furthermore, we can use --check-level=exhaustive on MacOS
# since there we have new newest version (2.11), while on RaspPi we still have 2.3.
#

CPPOPTIONS= --inline-suppr --enable=all --suppress=constParameterCallback --suppress=missingIncludeSystem
CPPINCLUDES:=$(shell echo $(INCLUDES) | sed -e "s/-pthread / /" )

ifeq ($(UNAME_S), Darwin)
CPPOPTIONS += -D__APPLE__ --check-level=exhaustive
else
CPPOPTIONS += -D__linux__
endif

.PHONY:	cppcheck
cppcheck:
	cppcheck $(CPPOPTIONS) $(OPTIONS) $(CPPINCLUDES) $(AUDIO_SOURCES) $(SOURCES) $(USBOZY_SOURCES) \
	$(SOAPYSDR_SOURCES) $(MIDI_SOURCES) $(STEMLAB_SOURCES) $(SERVER_SOURCES) $(SATURN_SOURCES)

.PHONY:	clean
clean:
	-rm -f src/*.o
	-rm -f $(PROGRAM) hpsdrsim bootloader
	-rm -rf $(PROGRAM).app
	-make -C release/LatexManual clean

.PHONY:	release
release: $(PROGRAM)
	make -C release/LatexManual release
	cp $(PROGRAM) release/pihpsdr
	cp /usr/local/lib/libwdsp.so release/pihpsdr
	cd release; tar cvf pihpsdr.tar pihpsdr
	cd release; tar cvf pihpsdr-$(GIT_VERSION).tar pihpsdr

#############################################################################
#
# hpsdrsim is a cool program that emulates an SDR board with UDP and TCP
# facilities. It even feeds back the TX signal and distorts it, so that
# you can test PureSignal.
# This feature only works if the sample rate is 48000
#
#############################################################################

src/hpsdrsim.o:     src/hpsdrsim.c  src/hpsdrsim.h
	$(CC) -c $(CFLAGS) -o src/hpsdrsim.o src/hpsdrsim.c
	
src/newhpsdrsim.o:	src/newhpsdrsim.c src/hpsdrsim.h
	$(CC) -c $(CFLAGS) -o src/newhpsdrsim.o src/newhpsdrsim.c

hpsdrsim:       src/hpsdrsim.o src/newhpsdrsim.o
	$(LINK) -o hpsdrsim src/hpsdrsim.o src/newhpsdrsim.o -lm


#############################################################################
#
# bootloader is a small command-line program that allows to
# set the radio's IP address and upload firmware through the
# ancient protocol. This program can only be run as root since
# this protocol requires "sniffing" at the Ethernet adapter
# (this "sniffing" is done via the pcap library)
#
#############################################################################

bootloader:	src/bootloader.c
	$(CC) -o bootloader src/bootloader.c -lpcap

#debian:
#	mkdir -p pkg/pihpsdr/usr/local/bin
#	mkdir -p pkg/pihpsdr/usr/local/lib
#	mkdir -p pkg/pihpsdr/usr/share/pihpsdr
#	mkdir -p pkg/pihpsdr/usr/share/applications
#	cp $(PROGRAM) pkg/pihpsdr/usr/local/bin
#	cp /usr/local/lib/libwdsp.so pkg/pihpsdr/usr/local/lib
#	cp release/pihpsdr/hpsdr.png pkg/pihpsdr/usr/share/pihpsdr
#	cp release/pihpsdr/hpsdr_icon.png pkg/pihpsdr/usr/share/pihpsdr
#	cp release/pihpsdr/pihpsdr.desktop pkg/pihpsdr/usr/share/applications
#	cd pkg; dpkg-deb --build pihpsdr

#############################################################################
#
# This is for MacOS "app" creation ONLY
#
#       The piHPSDR working directory is
#	$HOME -> Application Support -> piHPSDR
#
#       That is the directory where the WDSP wisdom file (created upon first
#       start of piHPSDR) but also the radio settings and the midi.props file
#       are stored.
#
#       No libraries are included in the app bundle, so it will only run
#       on the computer where it was created, and on other computers which
#       have all libraries (including WDSP) and possibly the SoapySDR support
#       modules installed.
#############################################################################

.PHONY: app
app:	$(OBJS) $(AUDIO_OBJS) $(USBOZY_OBJS)  $(SOAPYSDR_OBJS) \
		$(MIDI_OBJS) $(STEMLAB_OBJS) $(SERVER_OBJS) $(SATURN_OBJS)
	$(LINK) -headerpad_max_install_names -o $(PROGRAM) $(OBJS) $(AUDIO_OBJS) $(USBOZY_OBJS)  \
		$(SOAPYSDR_OBJS) $(MIDI_OBJS) $(STEMLAB_OBJS) $(SERVER_OBJS) $(SATURN_OBJS) \
		$(LIBS) $(LDFLAGS)
	@rm -rf pihpsdr.app
	@mkdir -p pihpsdr.app/Contents/MacOS
	@mkdir -p pihpsdr.app/Contents/Frameworks
	@mkdir -p pihpsdr.app/Contents/Resources
	@cp pihpsdr pihpsdr.app/Contents/MacOS/pihpsdr
	@cp MacOS/PkgInfo pihpsdr.app/Contents
	@cp MacOS/Info.plist pihpsdr.app/Contents
	@cp MacOS/hpsdr.icns pihpsdr.app/Contents/Resources/hpsdr.icns
	@cp MacOS/hpsdr.png pihpsdr.app/Contents/Resources
#############################################################################
