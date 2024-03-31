include libaxidma/library.mk

APP_DIR = $(ROOT)/app

SOURCE_FILES = main.cpp util.cpp aes.cpp directory_navigator.hpp adau1761.cpp virtual_file_wrapper.cpp player_thread.cpp aes_thread.cpp ui_thread.cpp
SOURCE_FILE_PATHS = $(addprefix $(APP_DIR)/,$(SOURCE_FILES))

APP_CXXFLAGS = $(GLOBAL_CFLAGS) -pthread
APP_LINKER_FLAGS = -Wl,-rpath,'$$ORIGIN/../lib'
APP_LIB_FLAGS = -L $(OUTPUT_DIR)/lib -l axidma -l mpg123 -l mp3lame -l sndfile $(APP_LINKER_FLAGS)


.PHONY: all app app_clean $(EXAMPLES_TARGETS) \
		$(EXAMPLES_CLEAN_TARGETS)

.SECONDEXPANSION:

app: $(OUTPUT_DIR)/bin/$$@

$(OUTPUT_DIR)/bin/app: $(SOURCE_FILE_PATHS) | libaxidma
	$(CXX) $(CXXFLAGS) $(APP_CXXFLAGS) -I$(INCLUDE_DIR) $(filter %.cpp,$^) -o $(OUTPUT_DIR)/bin/app $(APP_LIB_FLAGS)

app-clean:
	rm $(OUTPUT_DIR)/bin/app
