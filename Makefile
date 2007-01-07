
.PHONY: all clean

all: jmswm

clean:
	rm -f $(OBJECTS) libdraw.a libutil.a libmenu.a jmswm

UTIL_OBJECTS := util/close_on_exec.o util/log.o util/event.o

DRAW_OBJECTS := draw/draw.o

MENU_OBJECTS := menu/menu.o

WM_OBJECTS := wm/client.o wm/frame.o wm/view.o wm/xwindow.o wm/event.o wm/main.o \
              wm/wm.o wm/key.o wm/persistence.o wm/sizehint.o wm/bar.o \
              wm/volume.o

OBJECTS := $(UTIL_OBJECTS) $(DRAW_OBJECTS) $(WM_OBJECTS) $(MENU_OBJECTS)

SOURCES := $(OBJECTS:%.o=%.cpp)

LDLIBS := -levent -lboost_serialization -lboost_signals -lboost_thread \
          -lX11 -lXrandr $(shell pkg-config --libs pango xft pangoxft alsa)

CXXFLAGS += -g -Wall -Werror $(shell pkg-config --cflags pango xft pangoxft alsa) \
            -pthread

CPPFLAGS += -I.

$(OBJECTS): Makefile

%.a:
	$(AR) rc $@ $(filter %.o,$^)


libdraw.a: $(DRAW_OBJECTS)
libmenu.a: $(MENU_OBJECTS)
libutil.a: $(UTIL_OBJECTS)

jmswm: $(WM_OBJECTS) libdraw.a libutil.a libmenu.a
	$(LINK.cpp) $(WM_OBJECTS) libdraw.a libutil.a libmenu.a $(LOADLIBES) $(LDLIBS) -o $@

$(OBJECTS:.o=.d): %.d: %.cpp Makefile
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -M -MF $@ -MP $< -MT $(@:.d=.o)

include $(OBJECTS:.o=.d)
