TARGET = socks4d
CXXC = @g++

CXXFLAGS = -std=c++20 \
					 -Wall \
					 -Werror \
					 -Wextra \
					 -Wfatal-errors \
					 -Wno-deprecated-coroutine \
					 -I$(SRC_DIR)
					 
DEFINES = -DBOOST_LOG_DYN_LINK \
					-DIDENT_MAX_LEN=256  \
					-DDOMAIN_MAX_LEN=256 \
					-DTRANSFER_MAX_LEN=4096 \
					-DREQUEST_TIMEOUT=120 \
					-DTRANSFER_TIMEOUT=30

ifndef $(CONF)
	CONF := release
endif

ifeq ($(CONF), debug)
	CXXFLAGS += -g -ggdb -O0
	DEFINES += -DDEBUG
else
	CXXFLAGS += -O3
endif

LDFLAGS = -lboost_system \
					-lboost_filesystem \
					-lboost_program_options \
					-lboost_log \
					-lboost_log_setup \
					-lboost_thread 

DEPFLAGS = -MT $@ -MMD -MP -MF $(DEP_DIR)/$*.Td
POSTCOMPILE = @-mv -f $(DEP_DIR)/$*.Td $(DEP_DIR)/$*.d && touch $@

SRC_DIR := src
INC_DIR := include
DEP_DIR = dep
OBJ_DIR := obj

SRCS := $(shell find $(SRC_DIR) -name *.cpp -print)
INCS := $(shell find $(SRC_DIR) -name *.h -print)
OBJS := $(subst $(SRC_DIR), $(OBJ_DIR), $(SRCS:.cpp=.o))
DEPS := $(subst $(SRC_DIR), $(DEP_DIR), $(SRCS:.cpp=.d))

.PHONY: default
default: all

.PHONY: all
all: $(TARGET)

.PHONY: debug
debug: cleanall
	@$(MAKE) CONF=debug

$(TARGET): $(OBJS)
	@-echo "Building $@"
	$(CXXC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(DEP_DIR)/%.d | $(DEP_DIR)
	@-echo "Building $<"
	@-mkdir -p $(shell dirname $@)
	$(CXXC) $(DEPFLAGS) $(DEFINES) $(CXXFLAGS) -c $< -o $@
	$(POSTCOMPILE)

.PHONY: clean
clean:
	-@rm -rf $(OBJ_DIR)

.PHONY: cleanall
cleanall: clean
	-@rm -rf $(DEP_DIR) $(INC_DIR) $(TARGET)

.PHONY: distclean
cleantarget: clean
	-@rm -rf $(TARGET)

.PHONY: install
install: $(INCS)
	$(foreach i, $(INCS), $(shell mkdir -p $(shell dirname $(subst $(SRC_DIR), $(INC_DIR), $(i)))))
	$(foreach i, $(INCS), $(shell ln -sf $(i) $(subst $(SRC_DIR), $(INC_DIR), $(i))))
	@

$(OBJ_DIR):
	@-mkdir -p $(OBJ_DIR)

$(DEP_DIR):
	@-mkdir -p $(DEP_DIR)

$(DEPS):

include $(wildcard $(DEPS))

