
FINAL_TARGET := jmswm
FINAL_TARGET_STATIC := jmswm.static

.PHONY: all clean

all: $(FINAL_TARGET) $(FINAL_TARGET_STATIC)


CONFIG := build-config

include $(CONFIG)/components
include $(CONFIG)/compile-flags
include $(CONFIG)/link-flags

include $(foreach component,$(COMPONENTS),$(CONFIG)/$(component)-objects)

COMPONENT_LIBS := $(foreach component,$(COMPONENTS),lib$(component).a)

OBJECTS := $(foreach component,$(COMPONENTS),$($(shell echo "$(component)" | tr 'a-z' 'A-Z')_OBJECTS))

SOURCES := $(OBJECTS:%.o=%.cpp)

clean:
	rm -f $(OBJECTS) $(COMPONENT_LIBS) $(FINAL_TARGET)

veryclean: clean
	rm -f $(COMPONENT_LIBS:.a=.d) $(OBJECTS:.o=.d)

$(OBJECTS): Makefile $(CONFIG)/compile-flags

# PCH support does not work with anonymous namespaces until GCC 4.2.0

# wm/all.hpp.gch: wm/all.hpp Makefile
#	g++ -o $@ -x c++-header -c wm/all.hpp $(CPPFLAGS) $(CXXFLAGS)

# $(OBJECTS): wm/all.hpp.gch

$(COMPONENT_LIBS:.a=.d): lib%.d: $(CONFIG)/%-objects Makefile
	echo "$(@:.d=.a): $($(shell echo "$*" | tr 'a-z' 'A-Z')_OBJECTS)" > "$@"

$(COMPONENT_LIBS): lib%.a: Makefile
	$(AR) rc $@ $($(shell echo "$*" | tr 'a-z' 'A-Z')_OBJECTS)

$(FINAL_TARGET): $(COMPONENT_LIBS) $(CONFIG)/link-flags Makefile $(CONFIG)/components
	$(LINK.cpp) $(COMPONENT_LIBS) $(COMPONENT_LIBS) $(LOADLIBES) $(LDLIBS) -o $@

$(FINAL_TARGET_STATIC): $(COMPONENT_LIBS) $(CONFIG)/link-flags Makefile $(CONFIG)/components
	$(LINK.cpp) -static $(COMPONENT_LIBS) $(COMPONENT_LIBS) $(LOADLIBES) $(LDLIBS) -o $@

$(OBJECTS:.o=.d): %.d: %.cpp Makefile $(CONFIG)/compile-flags
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -M -MF $@ -MP $< -MT $(@:.d=.o)

#ifneq ($(MAKECMDGOALS),clean)
#ifneq ($(MAKECMDGOALS),veryclean)
-include $(OBJECTS:.o=.d)
-include $(COMPONENT_LIBS:.a=.d)
#endif
#endif
