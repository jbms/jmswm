
.PHONY: all clean

all: jmswm

clean:
	rm -f $(OBJECTS) libdraw.a libutil.a jmswm

UTIL_OBJECTS := util/close_on_exec.o util/log.o

DRAW_OBJECTS := draw/draw.o

WM_OBJECTS := wm/client.o wm/frame.o wm/view.o wm/xwindow.o wm/event.o wm/main.o \
              wm/wm.o wm/key.o

OBJECTS := $(UTIL_OBJECTS) $(DRAW_OBJECTS) $(WM_OBJECTS)

SOURCES := $(OBJECTS:%.o=%.cpp)

LDFLAGS += -levent

LDFLAGS += -lX11 $(shell pkg-config --libs pango xft pangoxft)

CXXFLAGS += -g -Wall -Werror $(shell pkg-config --cflags pango xft pangoxft)

CPPFLAGS += -I. -I/home/jbms/src/boost -I/home/jbms/src/boost-sandbox

$(OBJECTS): Makefile

%.a:
	$(AR) rc $@ $(filter %.o,$^)


libdraw.a: $(DRAW_OBJECTS)
libutil.a: $(UTIL_OBJECTS)

jmswm: $(WM_OBJECTS) libdraw.a libutil.a
	$(LINK.cpp) -o $@ $(WM_OBJECTS) libdraw.a libutil.a

$(OBJECTS:.o=.d): %.d: %.cpp Makefile
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -M -MF $@ -MP $< -MT $(@:.d=.o)

include $(OBJECTS:.o=.d)
